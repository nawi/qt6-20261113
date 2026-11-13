# NawiDBMan - Plan

Legenda: `[ ]` belum, `[x]` selesai + lulus test (bukti: unit test CI hijau dan/atau screenshot device).
Aturan: item hanya dicentang setelah teruji. Bukti UI disimpan di `docs/evidence/<fase>/`.

## 0. Keputusan (terkunci)

| Area | Keputusan |
|---|---|
| Stack | Qt 6.8 LTS, QML + C++ backend, build penuh di GitHub Actions |
| Target | arm64-v8a saja, minSdk 31 (Android 12), device uji: Samsung A055F, Android 15, 720x1600 @300dpi |
| Package / nama | `com.nawi.dbman` / NawiDBMan |
| UI | Qt Quick Controls style `Basic` + komponen custom bergaya iOS. Bahasa UI: English (string lewat `qsTr()`) |
| Tema | Light/Dark ikut sistem, bisa di-override di Settings. Warna aksen dibaca dari Android 12 dynamic color (`system_accent1_*`) via JNI |
| DB driver | MariaDB Connector/C (static, cross-compile di CI) dipakai langsung dari C++, tanpa `QtSql` untuk server remote |
| SSH | Rust `russh` (staticlib + C FFI, build pakai `cargo-ndk`). Auth: password atau private key tanpa passphrase. Host key TOFU. 1 tunnel dibagi semua tab dari koneksi yang sama (listener `127.0.0.1:<port acak>`) |
| PHP agent | JSON over HTTP POST, auth shared token, PHP 7.4+, kode di `agent/` repo ini |
| Storage | SQLite lokal (QtSql `QSQLITE`). Password dan private key dienkripsi AES-GCM dengan kunci Android Keystore |
| Test | QtTest (non-UI) di CI vs service container MariaDB. UI: adb + screenshot/scrcpy |
| Privilege | `SHOW`/`INFORMATION_SCHEMA` ditolak server: tampilkan notifikasi "privilege tidak cukup" |

## 1. Arsitektur singkat

```
QML (UI)  <->  C++ view-models (QAbstractListModel / QObject)
                  |
            DbSession (1 worker QThread per tab, API async via signal)
                  |
        IBackend  --+-- NativeBackend (libmariadb, TCP)  <- direct atau via SshTunnel
                    +-- HttpBackend   (QNetworkAccessManager -> agent PHP)
SshTunnel (Rust russh, 1 per koneksi, refcount per tab)
Storage: ConnectionStore, SettingsStore, HistoryStore, TabStore (SQLite) + SecretVault (Keystore)
```

`IBackend` ada karena memang 2 implementasi (native dan HTTP). Hasil query seragam: `ResultSet{columns[name,type], rows[], affected, insertId, truncated}`.

Layout repo:
```
CMakeLists.txt  android/ (manifest, res, Java KeystoreHelper)  src/ qml/ rust/ssh_bridge/
agent/tunnel.php  tests/  third_party/ (CMake fetch libmariadb, openssl)  .github/workflows/
```

## 2. Aturan perilaku (dari instruksi)

- Nama database wajib di koneksi. Semua list objek dibatasi database itu.
- Edit/hapus baris hanya untuk tabel dengan primary key. Tanpa PK = grid read-only.
- Sel diedit sebagai teks. Editor sel punya toggle "Set NULL" (supaya NULL beda dari string kosong).
- `DELETE` ditolak tanpa `WHERE`. Selalu tampil preview SQL + konfirmasi sebelum eksekusi.
- Tidak ada fitur DROP/TRUNCATE/ALTER di UI.
- SQL editor: 1 statement per run (tanpa flag multi-statement), tanpa highlight/autocomplete. Header menampilkan koneksi aktif (nama, host:port, db, mode tunnel).
- Hasil query read-only grid. Batas baris default 100 (setting global). Diterapkan dengan `SET SESSION sql_select_limit=N+1` dan flag `truncated`. Timeout default 300 detik (setting global), cancel pakai `KILL QUERY` dari koneksi kedua.
- Paginasi data grid: ukuran halaman di Settings. Total baris tidak dihitung (hanya "ada halaman berikut" lewat fetch N+1).
- Riwayat query: simpan per koneksi, bisa copy, jalankan ulang, hapus.
- Tab dan state-nya disimpan ke SQLite. Pulih setelah app di-background atau proses dimatikan Android (koneksi dibuka ulang otomatis).

## 3. Fase

### F0 - Fondasi CI dan bukti toolchain
Catatan: Qt 6.8.3 = versi 6.8 terbaru yang tersedia di aqtinstall (6.8.4/6.8.5 ada di mirror arsip tapi belum di repo aqt). NDK 26.1.10909125, platform/build-tools 36, JDK 17. Bukti: `docs/evidence/F0/hello.png` (run CI 43478250025).
- [x] Verifikasi dokumentasi terbaru via Context7: versi patch Qt 6.8.x terbaru, NDK (26.1.10909125 atau 27.2.12479018), platform/build-tools android-36, Gradle/AGP
- [x] Rewrite `android-build.yml`: Qt host `gcc_64` + target `android_arm64_v8a` (`--autodesktop`), hapus step cache ganda, JDK 17, NDK sesuai Qt, `QT_HOST_PATH`
- [x] Skeleton CMake + QML "Hello" + manifest (minSdk 31, package `com.nawi.dbman`)
- [x] Keystore tetap (dibuat lokal sekali, disimpan sebagai GitHub secret) agar APK bisa di-update tanpa uninstall; sign release APK
- [ ] Cross-compile OpenSSL + MariaDB Connector/C (static) di CI, cache hasil build
- [ ] Build `russh` staticlib arm64 (cargo-ndk), link ke app, panggil 1 fungsi FFI
- [ ] Job `unit-tests` terpisah (Qt desktop) + service MariaDB
- [x] Artifact APK diunduh `gh run download`, di-install `adb install -r`, layar Hello tampil di device
- [ ] Layar Hello menampilkan versi libmariadb dan russh (setelah keduanya terlink)
- Bukti: screenshot device

### F1 - Shell UI, tema, onboarding, daftar koneksi
- [ ] Ikon aplikasi custom (adaptive icon: silinder DB + node koneksi), bukan ikon standar
- [ ] Komponen iOS-style: NavBar (large title, back chevron), grouped inset list, action sheet, bottom sheet, swipe action, toggle/segmented
- [ ] `Theme` singleton: light/dark, aksen dari Android dynamic color (JNI), override di Settings
- [ ] Onboarding 3 slide, tampil sekali (flag di SQLite)
- [ ] SQLite schema + migrasi, `SecretVault` (Keystore AES-GCM) + QtTest enkripsi
- [ ] Daftar koneksi: card info (nama, host/db, badge mode Direct/SSH/HTTP, last used), tombol edit, tambah, hapus
- [ ] Form koneksi: nama, mode, host, port, user, password, database (wajib), SSL Off/On, field SSH, field HTTP, tombol Test + Save (validasi input)
- [ ] Settings global: tema, page size, row limit, query timeout
- Bukti: screenshot light/dark, phone + rotasi landscape

### F2 - Koneksi direct + Test Connection
- [ ] `NativeBackend` (connect, query, error mapping, charset utf8mb4 dengan fallback, opsi SSL, timeout connect/read)
- [ ] Test Connection: tampil versi server, hasil cek privilege, pesan error jelas
- [ ] QtTest vs MariaDB container (connect, query, error, SSL off)
- Bukti: Test Connection ke server nyata dari device

### F3 - SSH tunnel (wajib)
- [ ] Crate `ssh_bridge`: connect, auth password, auth private key (OpenSSH/PEM tanpa passphrase), `direct-tcpip` forward ke listener lokal, TOFU host key
- [ ] Dialog fingerprint (setujui/tolak), simpan di SQLite, tolak + peringatkan jika berubah
- [ ] `SshTunnel` manager: refcount per koneksi, reconnect, keepalive
- [ ] Form koneksi mode SSH + Test Connection via tunnel
- [ ] Integration test (sshd container + MariaDB) bila memungkinkan
- Bukti: konek ke server nyata via SSH dari device

### F4 - Daftar objek
- [ ] Buka koneksi dari kartu, indikator progress, error jelas
- [ ] Daftar Tables, Views, Functions, Procedures, Triggers (INFORMATION_SCHEMA, fallback `SHOW`)
- [ ] Notifikasi privilege tidak cukup per kelompok objek
- [ ] Segment/section + pencarian lokal ringan pada daftar
- Bukti: screenshot dengan user ber-privilege penuh dan terbatas

### F5 - Object design (read-only)
- [ ] Tab Fields: nama, tipe, null, default, key, extra, comment
- [ ] Tab Indexes (dan foreign key bila ada)
- [ ] Tab DDL: `SHOW CREATE TABLE/VIEW/FUNCTION/PROCEDURE/TRIGGER`, salin teks

### F6 - Data grid
- [ ] Grid scroll 2 arah, header tetap, lebar kolom otomatis, `QAbstractTableModel`
- [ ] Paginasi sesuai Settings
- [ ] Deteksi PK (`SHOW KEYS`), tanpa PK = read-only
- [ ] Edit sel inline (teks, toggle NULL) -> `UPDATE ... WHERE pk LIMIT 1`, tampil error
- [ ] Add row (form berdasar kolom) -> `INSERT`
- [ ] Delete row: preview SQL + konfirmasi, wajib punya `WHERE` PK

### F7 - Filter grid
- [ ] Builder: kolom + operator (`=, !=, <, >, <=, >=, LIKE, IS NULL, IS NOT NULL, IN`) + nilai, gabung AND/OR
- [ ] Query builder aman (identifier di-quote, nilai di-escape/parameter) + QtTest

### F8 - SQL editor
- [ ] Editor teks polos + header koneksi aktif
- [ ] Run/Cancel, timeout dan row limit dari Settings, indikator waktu eksekusi dan jumlah baris
- [ ] Hasil di grid read-only, pesan affected rows / error
- [ ] Guard: `DELETE`/`UPDATE` tanpa `WHERE` ditolak, `DROP`/`TRUNCATE` diblokir, preview SQL DELETE/UPDATE + konfirmasi
- [ ] Tab/halaman Riwayat: daftar, copy, load ke editor, run ulang, hapus

### F9 - Multi-tab + layout tablet
- [ ] Tab manager: banyak tab per koneksi (grid tabel, SQL editor, object design), tab bar di bawah
- [ ] 1 tunnel dibagi semua tab (refcount), tiap tab 1 `DbSession`
- [ ] Persist tab ke SQLite dan restore setelah proses dimatikan
- [ ] Tablet / layar lebar (>= 600dp): split view, daftar objek kiri, tab kanan
- Bukti: screenshot phone dan emulasi lebar tablet (`wm size` override)

### F10 - PHP tunnel agent
- [ ] `agent/tunnel.php` (PHP 7.4+, `mysqli`, `json`): aksi `ping`, `query`; token via header, `hash_equals`
- [ ] Protokol JSON: request `{action, db:{user,pass,name,charset,ssl?}, sql, limit, timeout}`, response `{ok, columns, rows, affected, insertId, truncated, error:{code,msg}}`. Binary/non-UTF8 di-base64 dengan flag kolom
- [ ] Agent hanya konek ke `localhost` kecuali `ALLOW_REMOTE_HOST=true` di config (cegah SSRF)
- [ ] `HttpBackend` + form koneksi mode HTTP, Test Connection
- [ ] QtTest + `php -S` vs MariaDB container; peringatan jika URL bukan HTTPS
- Bukti: konek via agent dari device

### F11 - Fitur akhir
- [ ] Sort kolom (server side `ORDER BY`)
- [ ] Pencarian di grid
- [ ] Export CSV (share sheet Android)

## 4. Keputusan tambahan (terkonfirmasi)
- Signing: keystore tetap dibuat lokal di `~/.android-keys/` (di luar repo), di-upload ke GitHub secrets via `gh`.
- SQL editor: `DELETE`/`UPDATE` tanpa `WHERE` ditolak. `DROP` dan `TRUNCATE` diblokir. `ALTER`/`CREATE`/`INSERT` boleh.
- SSL On = terenkripsi tanpa verifikasi sertifikat (Off = plaintext).
- Bahasa UI English. Persist tab ke SQLite. Host key SSH TOFU. PHP agent min 7.4.

## 5. Risiko tercatat
- OpenSSL 3 menolak TLS 1.0/1.1 dan cipher lama. MariaDB lama dengan SSL bisa gagal handshake, mitigasi: mode SSL Off, atau OpenSSL `@SECLEVEL=0` per koneksi bila perlu.
- Waktu build CI pertama lama (OpenSSL + Connector/C + Rust). Mitigasi: cache per versi.
- `caching_sha2_password` butuh plugin auth di Connector/C. Pastikan dikompilasi statis.
- Tabel besar tanpa total count: UX paginasi hanya Next/Prev.
