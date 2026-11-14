//! C FFI bridge over russh: password/key auth and a local `direct-tcpip` tunnel.
//!
//! The Qt side calls `ssh_bridge_connect` from a worker thread. It blocks until the
//! SSH session is up and a random local port is listening, then a Rust thread keeps
//! serving forwarded TCP connections until `ssh_bridge_disconnect` is called.
use std::collections::HashMap;
use std::ffi::{c_char, CStr};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{mpsc, Arc, Mutex, OnceLock};
use std::thread::JoinHandle;
use std::time::Duration;

use russh::client;
use russh::keys::{decode_secret_key, PrivateKeyWithHashAlg, PublicKeyOrCertificate};
use russh::ChannelMsg;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

/// Result codes shared with C++ (`SshTunnel::ErrorCode`).
const RC_OK: i32 = 0;
const RC_NETWORK: i32 = 1;
const RC_AUTH: i32 = 2;
const RC_HOSTKEY_UNKNOWN: i32 = 3;
const RC_HOSTKEY_MISMATCH: i32 = 4;
const RC_OTHER: i32 = 5;
const RC_BAD_KEY: i32 = 6;

struct Handler {
    expected: Option<String>,
    actual: Arc<Mutex<Option<String>>>,
}

impl client::Handler for Handler {
    type Error = russh::Error;

    async fn check_server_key(
        &mut self,
        key: &PublicKeyOrCertificate,
    ) -> Result<bool, Self::Error> {
        let fp = match key {
            PublicKeyOrCertificate::PublicKey { key, .. } => {
                key.fingerprint(Default::default()).to_string()
            }
            PublicKeyOrCertificate::Certificate(cert) => {
                cert.public_key().fingerprint(Default::default()).to_string()
            }
        };
        if let Ok(mut slot) = self.actual.lock() {
            *slot = Some(fp.clone());
        }
        match &self.expected {
            None => Ok(false), // ask the UI (TOFU)
            Some(want) => Ok(want == &fp),
        }
    }
}

struct Tunnel {
    stop: tokio::sync::oneshot::Sender<()>,
    thread: JoinHandle<()>,
}

static TUNNELS: OnceLock<Mutex<HashMap<u64, Tunnel>>> = OnceLock::new();
static NEXT_ID: AtomicU64 = AtomicU64::new(1);

fn tunnels() -> &'static Mutex<HashMap<u64, Tunnel>> {
    TUNNELS.get_or_init(|| Mutex::new(HashMap::new()))
}

struct Setup {
    rc: i32,
    port: u16,
    fingerprint: String,
    message: String,
}

async fn authenticate(
    session: &mut client::Handle<Handler>,
    user: &str,
    auth_kind: i32,
    secret: &str,
) -> Result<i32, String> {
    if auth_kind == 1 {
        let key = decode_secret_key(secret, None).map_err(|e| format!("Invalid private key: {e}"))?;
        let hash = session
            .best_supported_rsa_hash()
            .await
            .map_err(|e| e.to_string())?
            .flatten();
        let res = session
            .authenticate_publickey(user, PrivateKeyWithHashAlg::new(Arc::new(key), hash))
            .await
            .map_err(|e| e.to_string())?;
        if res.success() { Ok(RC_OK) } else { Ok(RC_AUTH) }
    } else {
        let res = session
            .authenticate_password(user, secret)
            .await
            .map_err(|e| e.to_string())?;
        if res.success() { Ok(RC_OK) } else { Ok(RC_AUTH) }
    }
}

async fn setup(
    host: &str,
    port: u16,
    user: &str,
    auth_kind: i32,
    secret: &str,
    expected: Option<String>,
    db_host: String,
    db_port: u16,
    timeout: Duration,
) -> (Setup, Option<TcpListener>, Option<Arc<client::Handle<Handler>>>) {
    let actual = Arc::new(Mutex::new(None));
    let handler = Handler { expected: expected.clone(), actual: actual.clone() };
    let config = Arc::new(client::Config {
        inactivity_timeout: Some(Duration::from_secs(300)),
        nodelay: true,
        ..Default::default()
    });
    let addr = format!("{host}:{port}");
    let connect = tokio::time::timeout(timeout, client::connect(config, addr.as_str(), handler)).await;
    let mut session = match connect {
        Err(_) => {
            return (
                Setup { rc: RC_NETWORK, port: 0, fingerprint: String::new(), message: "SSH connect timed out".into() },
                None, None,
            )
        }
        Ok(Err(e)) => {
            let fp = actual.lock().ok().and_then(|g| g.clone()).unwrap_or_default();
            let (rc, msg) = if !fp.is_empty() {
                // A fingerprint was seen: the server was rejected either because it is new
                // (TOFU prompt) or because the known key changed.
                let is_unknown = expected.as_deref().map_or(true, |s| s.is_empty());
                let rc = if is_unknown { RC_HOSTKEY_UNKNOWN } else { RC_HOSTKEY_MISMATCH };
                (rc, if rc == RC_HOSTKEY_UNKNOWN { "Host key is not trusted yet".to_string() } else { "Host key changed".to_string() })
            } else {
                (RC_NETWORK, format!("SSH connect failed: {e}"))
            };
            return (Setup { rc, port: 0, fingerprint: fp, message: msg }, None, None);
        }
        Ok(Ok(s)) => s,
    };

    match authenticate(&mut session, user, auth_kind, secret).await {
        Ok(RC_OK) => {}
        Ok(rc) => {
            return (
                Setup { rc, port: 0, fingerprint: String::new(), message: "SSH authentication failed".into() },
                None, None,
            )
        }
        Err(e) => {
            return (
                Setup { rc: RC_OTHER, port: 0, fingerprint: String::new(), message: e },
                None, None,
            )
        }
    }

    let listener = match TcpListener::bind("127.0.0.1:0").await {
        Ok(l) => l,
        Err(e) => {
            return (
                Setup { rc: RC_OTHER, port: 0, fingerprint: String::new(), message: format!("Local listener failed: {e}") },
                None, None,
            )
        }
    };
    let port = listener.local_addr().map(|a| a.port()).unwrap_or(0);
    let fp = actual.lock().ok().and_then(|g| g.clone()).unwrap_or_default();
    let _ = (db_host, db_port);
    (Setup { rc: RC_OK, port, fingerprint: fp, message: String::new() }, Some(listener), Some(Arc::new(session)))
}

async fn forward(
    mut channel: russh::Channel<russh::client::Msg>,
    mut stream: TcpStream,
) {
    let mut buf = vec![0u8; 32768];
    loop {
        tokio::select! {
            msg = channel.wait() => match msg {
                Some(ChannelMsg::Data { data }) => {
                    if stream.write_all(&data).await.is_err() { break; }
                }
                Some(ChannelMsg::ExtendedData { data, .. }) => {
                    if stream.write_all(&data).await.is_err() { break; }
                }
                Some(ChannelMsg::Eof) | Some(ChannelMsg::Close) | None => break,
                _ => {}
            },
            n = stream.read(&mut buf) => match n {
                Ok(0) => { let _ = channel.eof().await; break; }
                Ok(n) => {
                    if channel.data(&buf[..n]).await.is_err() { break; }
                }
                Err(_) => break,
            },
        }
    }
    let _ = channel.close().await;
}

async fn serve(
    handle: Arc<client::Handle<Handler>>,
    listener: TcpListener,
    db_host: String,
    db_port: u16,
    mut stop: tokio::sync::oneshot::Receiver<()>,
) {
    loop {
        tokio::select! {
            _ = &mut stop => break,
            accepted = listener.accept() => {
                let Ok((stream, origin)) = accepted else { continue };
                let session = handle.clone();
                let db_host = db_host.clone();
                tokio::spawn(async move {
                    let Ok(channel) = session
                        .channel_open_direct_tcpip(
                            db_host,
                            db_port as u32,
                            origin.ip().to_string(),
                            origin.port() as u32,
                        )
                        .await
                    else { return };
                    forward(channel, stream).await;
                });
            }
        }
    }
}

fn copy_out(dst: *mut c_char, len: i32, text: &str) {
    if dst.is_null() || len <= 0 { return; }
    let bytes = text.as_bytes();
    let n = bytes.len().min(len as usize - 1);
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), dst.cast::<u8>(), n);
        *dst.add(n) = 0;
    }
}

fn cstr_arg<'a>(p: *const c_char) -> &'a str {
    if p.is_null() { return ""; }
    unsafe { CStr::from_ptr(p) }.to_str().unwrap_or("")
}

/// Starts a tunnel. Returns a handle > 0 on success, 0 on failure.
/// `expected_fp` empty = TOFU mode: an unknown host returns rc 3 with `fp_out` filled.
#[no_mangle]
pub extern "C" fn ssh_bridge_connect(
    host: *const c_char,
    port: u16,
    user: *const c_char,
    auth_kind: i32,
    secret: *const c_char,
    expected_fp: *const c_char,
    db_host: *const c_char,
    db_port: u16,
    timeout_sec: i32,
    local_port_out: *mut u16,
    fp_out: *mut c_char,
    fp_len: i32,
    rc_out: *mut i32,
    err_out: *mut c_char,
    err_len: i32,
) -> u64 {
    let host = cstr_arg(host).to_string();
    let user = cstr_arg(user).to_string();
    let secret = cstr_arg(secret).to_string();
    let expected_s = cstr_arg(expected_fp).trim().to_string();
    let expected = if expected_s.is_empty() { None } else { Some(expected_s) };
    let db_host = cstr_arg(db_host).to_string();
    let timeout = Duration::from_secs(timeout_sec.max(5) as u64);

    let (tx, rx) = mpsc::channel::<Setup>();
    let (stop_tx, stop_rx) = tokio::sync::oneshot::channel::<()>();
    let thread = std::thread::spawn(move || {
        let rt = match tokio::runtime::Builder::new_multi_thread()
            .worker_threads(2)
            .enable_all()
            .build()
        {
            Ok(rt) => rt,
            Err(e) => {
                let _ = tx.send(Setup { rc: RC_OTHER, port: 0, fingerprint: String::new(), message: e.to_string() });
                return;
            }
        };
        rt.block_on(async move {
            let (setup, listener, handle) = setup(
                &host, port, &user, auth_kind, &secret, expected, db_host.clone(), db_port, timeout,
            )
            .await;
            if let (Some(listener), Some(handle)) = (listener, handle) {
                let _ = tx.send(Setup { rc: setup.rc, port: setup.port, fingerprint: setup.fingerprint, message: setup.message });
                serve(handle, listener, db_host, db_port, stop_rx).await;
            } else {
                let _ = tx.send(setup);
            }
        });
    });

    let result = match rx.recv_timeout(timeout + Duration::from_secs(5)) {
        Ok(s) => s,
        Err(_) => Setup { rc: RC_NETWORK, port: 0, fingerprint: String::new(), message: "SSH setup timed out".into() },
    };
    if !rc_out.is_null() { unsafe { *rc_out = result.rc; } }
    if !local_port_out.is_null() { unsafe { *local_port_out = result.port; } }
    copy_out(fp_out, fp_len, &result.fingerprint);
    copy_out(err_out, err_len, &result.message);

    if result.rc != RC_OK {
        let _ = thread.join();
        return 0;
    }
    let id = NEXT_ID.fetch_add(1, Ordering::Relaxed);
    if let Ok(mut map) = tunnels().lock() {
        map.insert(id, Tunnel { stop: stop_tx, thread });
    }
    id
}

#[no_mangle]
pub extern "C" fn ssh_bridge_disconnect(id: u64) {
    let tunnel = tunnels().lock().ok().and_then(|mut m| m.remove(&id));
    if let Some(t) = tunnel {
        let _ = t.stop.send(());
        let _ = t.thread.join();
    }
}

/// Static NUL-terminated string, valid for program lifetime.
#[no_mangle]
pub extern "C" fn ssh_bridge_version() -> *const c_char {
    c"ssh_bridge 0.2.0 / russh 0.65".as_ptr()
}

/// Builds a russh client config on a tokio runtime so russh+tokio are really linked.
#[no_mangle]
pub extern "C" fn ssh_bridge_selftest() -> i32 {
    let Ok(rt) = tokio::runtime::Builder::new_current_thread().enable_all().build() else {
        return 0;
    };
    rt.block_on(async {
        let cfg = russh::client::Config::default();
        (cfg.inactivity_timeout.is_none() || cfg.inactivity_timeout.is_some()) as i32
    })
}

#[cfg(test)]
mod tests {
    #[test]
    fn selftest_ok() {
        assert_eq!(super::ssh_bridge_selftest(), 1);
    }
}
