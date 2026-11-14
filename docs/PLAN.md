# NawiDBMan - Plan

Legenda: `[ ]` belum, `[x]` selesai + lulus test (bukti: unit test CI hijau dan/atau screenshot device).
Aturan: item hanya dicentang setelah teruji. Bukti UI disimpan di `docs/evidence/<fase>/`.

## 0. Keputusan (terkunci)

| Area | Keputusan |
|---|---|
| Stack | Qt 6.12 (6.8 EOL), QML + C++ backend, build penuh di GitHub Actions |
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
- [x] Cross-compile OpenSSL + MariaDB Connector/C (static) di CI, cache hasil build
- [x] Build `russh` staticlib arm64 (cargo-ndk), link ke app, panggil 1 fungsi FFI
- [x] Job `unit-tests` terpisah (Qt desktop) + service MariaDB
- [x] Artifact APK diunduh `gh run download`, di-install `adb install -r`, layar Hello tampil di device
- [x] Layar Hello menampilkan versi libmariadb dan russh (setelah keduanya terlink)
- Bukti: `docs/evidence/F0/native-deps.png` (run CI 43479753547): Connector/C 3.4.11, OpenSSL 3.5.9, russh 0.65 jalan di A055F.
- Catatan: static plugin auth terkompilasi: `caching_sha2_password`, `sha256_password`, `mysql_native_password`, `mysql_old_password`, `mysql_clear_password`. Build deps pertama 4m19s, run berikut memakai cache. Test vs server nyata (QtTest `server`/`badPassword`) jalan di CI dengan MariaDB 10.11.
- Status F0: SELESAI.

### F0b - Migrasi Qt 6.8 -> 6.12 (diminta setelah F1)
Syarat Qt 6.12 (Context7 doc.qt.io/qt-6): NDK 27.2.12479018, JDK 21, platform android-36, build-tools 36.0.0, Gradle 9.5.1 / AGP 9.2.1 (dikelola Qt), template manifest baru (`QT_ANDROID_APP_ICON`).
- [x] INSTRUCTION.md dan PLAN.md menyebut Qt 6.12
- [x] CMake: Android wajib Qt 6.12, ikon via `QT_ANDROID_APP_ICON`, manifest template 6.12
- [x] Workflow: QT 6.12.0, NDK 27.2.12479018, JDK 21, unit test desktop memakai Qt 6.12 yang sama dengan app
- [x] CI hijau (unit test + APK)
- [x] Device: onboarding, daftar, form + keyboard, Test Connection, tombol Back, tema, ikon, inset status bar tetap benar di Qt 6.12
- Bukti: `docs/evidence/F0b/` (run CI 43494850377). APK memuat `libQt6Core` 6.12.0, dibangun dengan NDK 27.2, JDK 21.
- Temuan migrasi (dari dokumentasi Context7 + uji device): (1) Qt >= 6.9 `ApplicationWindow` menambah padding safe area otomatis -> inset ganda dengan inset buatan sendiri, dimatikan (`topPadding: 0` dst). (2) Qt 6.12 Back tidak lagi mengirim `Window.onClosing` (pakai `moveTaskToBack`) -> diganti `Shortcut { sequences: ["Back"] }`. (3) Ikon lewat `QT_ANDROID_APP_ICON`.
- Status F0b: SELESAI.



### F1 - Shell UI, tema, onboarding, daftar koneksi
- [x] Ikon aplikasi custom (adaptive icon: silinder DB + node koneksi), bukan ikon standar
- [x] Komponen iOS-style: NavBar (large title, back chevron), grouped inset list, action sheet, bottom sheet, toggle/segmented (swipe action ditunda ke F4 bila dibutuhkan)
- [x] `Theme` singleton: light/dark, aksen dari Android dynamic color (JNI), override di Settings
- [x] Onboarding 3 slide, tampil sekali (flag di SQLite)
- [x] SQLite schema + migrasi, `SecretVault` (Keystore AES-GCM) + QtTest enkripsi
- [x] Daftar koneksi: card info (nama, host/db, badge mode Direct/SSH/HTTP, last used), tombol edit, tambah, hapus
- [x] Form koneksi: nama, mode, host, port, user, password, database (wajib), SSL Off/On, field SSH, field HTTP, tombol Test + Save (validasi input)
- [x] Settings global: tema, page size, row limit, query timeout
- Bukti: `docs/evidence/F1/` (01-12): onboarding, daftar kosong dark/light, form, Test Connection OK (MariaDB 10.11 via `adb reverse`), password salah, validasi, edit (password terdekripsi Keystore setelah restart proses), delete action sheet, settings, landscape.
- Catatan: Test Connection mode SSH/HTTP baru aktif di F3/F10 (form sudah menyimpan field-nya). "Last used" terisi saat koneksi dibuka (F4). Keyboard: form menyusut di atas IME (`adjustNothing` + inset via JNI). Bug yang ditemukan di device dan diperbaiki: SSL Off ditolak Connector/C 3.4 (TLS wajib default) dan teks tombol tidak terbaca pada aksen terang.
- Status F1: SELESAI.

### F2 - Koneksi direct + Test Connection
Keputusan (usulan default, belum dibantah): charset `utf8mb4`, fallback `utf8` bila server menolak (error 2019). Test Connection juga menampilkan jumlah objek yang terlihat + catatan privilege. `IBackend` ditunda sampai ada implementasi kedua (F10); sekarang class konkret `DbClient`.
- [x] `DbClient`: connect, query -> `ResultSet` (kolom, baris, NULL vs string kosong, affected, insertId, truncated), batas baris via `sql_select_limit`, timeout connect/read, `local-infile` mati
- [x] Opsi SSL Off/On (On = terenkripsi tanpa verifikasi), tampilkan cipher TLS
- [x] Charset utf8mb4 dengan fallback utf8
- [x] Error mapping: privilege (1044/1142/1143/1227/1370), tidak terjangkau (2002/2003/2005), koneksi putus (2006/2013), lainnya pesan server
- [x] Test Connection: versi + jumlah Tables/Views/Functions/Procedures/Triggers (n/a bila tidak diizinkan)
- [x] QtTest vs MariaDB (connect, bad password 1045, unknown db 1049, unreachable, query, NULL vs '', utf8mb4, truncation, syntax error 1064, privilege 1044)
- [x] Bukti device: OK, password salah, DB tidak dikenal, server tidak terjangkau, privilege kurang, SSL On ke server ber-TLS
- Bukti: `docs/evidence/F1/04-test-ok.png`, `05-bad-password.png`, `06-validation.png` + `docs/evidence/F3-real/02-test-result.png`
- Status F2: SELESAI.

### F3 - SSH tunnel (wajib)
- [x] Crate `ssh_bridge`: connect, auth password, auth private key (OpenSSH/PEM tanpa passphrase), `direct-tcpip` forward ke listener lokal, TOFU host key
- [x] Dialog fingerprint (setujui/tolak), simpan di SQLite, tolak + peringatkan jika berubah
- [x] `SshTunnel` manager: refcount per koneksi, reconnect, keepalive
- [x] Form koneksi mode SSH + Test Connection via tunnel
- [x] Integration test (sshd container + MariaDB) di CI
- Bukti: `docs/evidence/F3-real/` 01-hostkey-dialog.png (TOFU), 02-test-result.png (Test Connection sukses ke MySQL 5.7.27 via SSH tunnel port 22000 dengan RSA private key).
- Status F3: SELESAI.

### F4 - Daftar objek
- [x] Buka koneksi dari kartu, indikator progress, error jelas
- [x] Daftar Tables, Views, Functions, Procedures, Triggers (INFORMATION_SCHEMA, fallback `SHOW`)
- [x] Notifikasi privilege tidak cukup per kelompok objek
- [x] Segment/section + pencarian lokal ringan pada daftar
- Bukti: `docs/evidence/F3-real/03-objects-ssh.png` (daftar 39 tabel server real via SSH), pencarian objek `vessel_tes`.
- Status F4: SELESAI.

### F5 - Object design (read-only)
- [x] Tab Fields: nama, tipe, null, default, key, extra, comment
- [x] Tab Indexes (dan foreign key bila ada)
- [x] Tab DDL: `SHOW CREATE TABLE/VIEW/FUNCTION/PROCEDURE/TRIGGER`, salin teks
- Bukti: `docs/evidence/F3-real/10-table-design.png` (Fields), `11-table-ddl.png` (DDL `vessel_tes` + tombol Copy).
- Status F5: SELESAI.

### F6 - Data grid
- [x] Grid scroll 2 arah, header tetap, lebar kolom otomatis, `QAbstractTableModel`
- [x] Paginasi sesuai Settings
- [x] Deteksi PK (`SHOW KEYS`), tanpa PK = read-only
- [x] Edit sel inline (teks, toggle NULL) -> `UPDATE ... WHERE pk LIMIT 1`, tampil error
- [x] Add row (form berdasar kolom) -> `INSERT`
- [x] Delete row: preview SQL + konfirmasi, wajib punya `WHERE` PK
- Bukti: `docs/evidence/F3-real/` 04-grid-vessel_tes.png, 05-edit-saved.png (`TES 01 edited`), 06-insert-done.png (`KM Uji Android`), 07-delete-confirm.png (preview `DELETE ... WHERE id='52405' LIMIT 1`).
- Status F6: SELESAI.

### F7 - Filter grid
- [x] Builder: kolom + operator (`=, !=, <, >, <=, >=, LIKE, IS NULL, IS NOT NULL, IN`) + nilai, gabung AND/OR
- [x] Query builder aman (identifier di-quote, nilai di-escape/parameter) + QtTest
- Bukti: `docs/evidence/F3-real/09-filter-applied-id1.png` (filter `id = 1` diaplikasikan ke grid).
- Bug diperbaiki: tinggi FilterSheet eksplisit berbasis baris kondisi untuk mencegah clipping/binding loop ANR.
- Status F7: SELESAI.

### F8 - SQL editor
- [x] Editor teks polos + header koneksi aktif
- [x] Run/Cancel, timeout dan row limit dari Settings, indikator waktu eksekusi dan jumlah baris
- [x] Hasil di grid read-only, pesan affected rows / error
- [x] Guard: `DELETE`/`UPDATE` tanpa `WHERE` ditolak, `DROP`/`TRUNCATE` diblokir, preview SQL DELETE/UPDATE + konfirmasi
- [x] Tab/halaman Riwayat: daftar, copy, load ke editor, run ulang, hapus
- Bukti: `docs/evidence/F3-real/` 12-sql-editor-open.png, 13-sql-editor-ran.png (`3 row(s) in 18 ms`), 14-sql-editor-guard-drop.png (guard "DROP and TRUNCATE are blocked"), 15-sql-history.png (riwayat query + reload ke editor).
- Status F8: SELESAI.

### F9 - Multi-tab + layout tablet
- [x] Tab manager: banyak tab per koneksi (grid tabel, SQL editor, object design), tab bar di bawah
- [x] 1 tunnel dibagi semua tab (refcount), tiap tab 1 `DbSession`
- [x] Persist tab ke SQLite dan restore setelah proses dimatikan
- [x] Tablet / layar lebar (>= 600dp): split view, daftar objek kiri, tab kanan
- Bukti: `docs/evidence/F3-real/08-reopen-tab-restored.png` (tab `vessel_tes` otomatis pulih setelah app di-kill), `16-tablet-split.png` (split view landscape/tablet >= 600dp: daftar objek di kiri, tab editor di kanan).
- Status F9: SELESAI.

### F10 - PHP tunnel agent
- [x] `agent/tunnel.php` (PHP 7.4+, `mysqli`, `json`): aksi `ping`, `query`; token via header `X-Nawi-Token`, `hash_equals`
- [x] Protokol JSON: request `{action, db:{host,port,user,pass,name}, sql, limit, timeout}`, response `{ok, columns, rows, affected, insertId, truncated, error:{code,msg}}`. Non-UTF8 jadi `base64:` (konvensi sama dengan klien native)
- [x] Agent hanya konek ke `localhost`/`127.0.0.1`/`::1` kecuali `ALLOW_REMOTE_HOST` (cegah SSRF)
- [x] `HttpAgent` (Qt Network) + `DbClient` mode HTTP + form koneksi mode HTTP + Test Connection; peringatan jika URL bukan HTTPS
- [x] QtTest `tst_http` (ping, NULL vs '', utf8, truncation, affected/insertId, 1064, 1045, token salah 401, host remote 403, agent tak terjangkau) vs `php -S` PHP 7.4 + MariaDB di CI
- [x] Android: `libssl_3.so`/`libcrypto_3.so` (KDAB android_openssl) dibundel via `QT_ANDROID_EXTRA_LIBS` agar Qt Network bisa HTTPS
- Bukti: `docs/evidence/F10-F11/` 01-07: Test Connection OK, daftar objek, grid lewat agent (PHP 7.4 di container, `adb reverse tcp:8099`).
- Catatan: KILL QUERY tidak tersedia lewat agent (1 koneksi DB per request), Cancel hanya menghentikan di sisi klien. Escape nilai pada mode HTTP memakai aturan default sql_mode.
- Bug ditemukan di device dan diperbaiki: simpan koneksi baru gagal `NOT NULL constraint failed: connections.ssh_fp` (QString null dari QML terikat sebagai SQL NULL). Regression test di `tst_storage`.
- Status F10: SELESAI.

### F11 - Fitur akhir
- [x] Sort kolom (server side `ORDER BY`, tap header: ASC, DESC, off)
- [x] Pencarian di grid (`CAST(col AS CHAR) LIKE`, wildcard di-escape, digabung dengan filter)
- [x] Export CSV (RFC 4180, share sheet Android via FileProvider) untuk grid tabel dan hasil SQL editor
- Bukti: `docs/evidence/F10-F11/` 08-sort-desc, 09-search, 10-share. QtTest: `sortAndSearch`, `csvExport`, `searchWhere`.
- Status F11: SELESAI.

### Status
- Seluruh fase F0, F0b, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11 telah selesai dieksekusi, lulus unit test CI dan terverifikasi penuh di device Samsung Galaxy A055F (Android 15, ARM64) baik via Direct TCP, HTTP Agent, maupun real SSH Tunnel dengan RSA private key.
- Bukti uji lengkap tersimpan di:
  - `docs/evidence/F0/`
  - `docs/evidence/F0b/`
  - `docs/evidence/F1/`
  - `docs/evidence/F10-F11/`
  - `docs/evidence/F3-real/` (01-16: TOFU host key, test connection, object list, grid data, edit cell, insert row, delete row + confirm preview, tab restore after kill, filter builder, table design fields & DDL, SQL editor run, SQL editor guard drop/truncate, SQL history, tablet split view >= 600dp).

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
