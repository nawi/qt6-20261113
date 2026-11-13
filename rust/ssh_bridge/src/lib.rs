//! C FFI bridge over russh. F0: only a link/version probe; tunnel API comes in F3.
use std::ffi::c_char;

/// Static NUL-terminated string, valid for program lifetime.
#[no_mangle]
pub extern "C" fn ssh_bridge_version() -> *const c_char {
    c"ssh_bridge 0.1.0 / russh 0.65".as_ptr()
}

/// Builds a russh client config on a tokio runtime so russh+tokio are really linked.
/// Returns 1 on success.
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
