<?php
/**
 * NawiDBMan PHP tunnel agent (PHP 7.4+, ext mysqli + json).
 * Deploy on the server that can reach the database, over HTTPS.
 *
 * Config: edit $config below, or set env NAWI_TOKEN / NAWI_ALLOW_REMOTE_HOST=1.
 * Auth:   header "X-Nawi-Token: <token>". Each request opens its own DB connection.
 * Body:   {"action":"ping|query","db":{"host","port","user","pass","name"},"sql":"","limit":-1,"timeout":300}
 */
$config = [
    'token' => 'change-me',          // REQUIRED: long random string
    'allow_remote_host' => false,    // false: only connect to localhost (prevents SSRF)
];
if (getenv('NAWI_TOKEN') !== false) $config['token'] = getenv('NAWI_TOKEN');
if (getenv('NAWI_ALLOW_REMOTE_HOST') === '1') $config['allow_remote_host'] = true;

mysqli_report(MYSQLI_REPORT_OFF);
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// ---- AES-256-GCM envelope -------------------------------------------------------
// WAFs in front of this agent block SQL keywords (e.g. any "WHERE", information_schema).
// The app can send/receive an encrypted envelope instead: {"v":1,"n":nonce,"d":ct,"t":tag}.
// Key = HKDF-SHA256(token, info "nawidbman-v1"), 32 bytes. AAD differs per direction.
global $ENC_KEY;
$ENC_KEY = null;

function cryptoKey(string $token): string
{
    return hash_hkdf('sha256', $token, 32, 'nawidbman-v1');
}

function seal(array $data, string $key): string
{
    $nonce = random_bytes(12);
    $tag = '';
    $plain = json_encode($data, JSON_INVALID_UTF8_SUBSTITUTE | JSON_PARTIAL_OUTPUT_ON_ERROR);
    $ct = openssl_encrypt($plain, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $nonce, $tag,
                          'nawidbman-v1/res', 16);
    if ($ct === false) return '';
    return json_encode(['v' => 1, 'n' => base64_encode($nonce),
                        'd' => base64_encode($ct), 't' => base64_encode($tag)]);
}

function unseal(array $env, string $key): ?array
{
    if ((int)($env['v'] ?? 0) !== 1) return null;
    $nonce = base64_decode((string)($env['n'] ?? ''), true);
    $ct = base64_decode((string)($env['d'] ?? ''), true);
    $tag = base64_decode((string)($env['t'] ?? ''), true);
    if ($nonce === false || $ct === false || $tag === false || strlen($nonce) !== 12 || strlen($tag) !== 16)
        return null;
    $plain = openssl_decrypt($ct, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $nonce, $tag,
                             'nawidbman-v1/req');
    if ($plain === false) return null;
    $req = json_decode($plain, true);
    if (!is_array($req)) return null;
    $ts = (int)($req['t'] ?? 0);
    if ($ts <= 0 || abs(time() - $ts) > 300) return null;   // anti-replay
    unset($req['t']);
    return $req;
}

function out(array $data, int $status = 200): void
{
    global $ENC_KEY;
    http_response_code($status);
    if ($ENC_KEY !== null) {
        $sealed = seal($data, $ENC_KEY);
        if ($sealed !== '') {
            header('Content-Type: application/json; charset=utf-8');
            echo $sealed;
            exit;
        }
    }
    echo json_encode($data, JSON_INVALID_UTF8_SUBSTITUTE | JSON_PARTIAL_OUTPUT_ON_ERROR);
    exit;
}

function fail(int $status, int $code, string $msg): void
{
    out(['ok' => false, 'error' => ['code' => $code, 'msg' => $msg]], $status);
}

function typeName(int $t): string
{
    static $names = [
        0 => 'decimal', 1 => 'tinyint', 2 => 'smallint', 3 => 'int', 4 => 'float', 5 => 'double',
        6 => 'null', 7 => 'timestamp', 8 => 'bigint', 9 => 'mediumint', 10 => 'date', 11 => 'time',
        12 => 'datetime', 13 => 'year', 14 => 'date', 15 => 'varchar', 16 => 'bit', 245 => 'json',
        246 => 'decimal', 247 => 'enum', 248 => 'set', 249 => 'tinyblob', 250 => 'mediumblob',
        251 => 'longblob', 252 => 'blob', 253 => 'varstring', 254 => 'char', 255 => 'geometry',
    ];
    return $names[$t] ?? ('type' . $t);
}

function isBinaryField($f): bool
{
    $blob = in_array($f->type, [249, 250, 251, 252, 255, 16], true);
    $stringy = in_array($f->type, [15, 245, 253, 254], true);
    return $blob || ($stringy && $f->charsetnr == 63);
}

function cell($v)
{
    if ($v === null) return null;
    $v = (string)$v;
    // same convention as the native client: invalid UTF-8 -> "base64:<data>"
    return mb_check_encoding($v, 'UTF-8') ? $v : 'base64:' . base64_encode($v);
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') fail(405, -1, 'POST required');

$token = (string)$config['token'];
if ($token === '' || $token === 'change-me') fail(500, -1, 'Agent token is not configured');
if (!hash_equals($token, (string)($_SERVER['HTTP_X_NAWI_TOKEN'] ?? ''))) fail(401, -1, 'Invalid token');

$req = json_decode((string)file_get_contents('php://input'), true);
if (!is_array($req)) fail(400, -1, 'Invalid JSON body');
if (isset($req['v'])) {
    // Encrypted envelope: authenticate with the same shared token, then decrypt.
    $ENC_KEY = cryptoKey($token);
    $plain = unseal($req, $ENC_KEY);
    if ($plain === null) {
        $ENC_KEY = null;   // reply in plaintext: the caller may not share our key
        fail(400, -1, 'Cannot decrypt request (token mismatch or stale timestamp)');
    }
    $req = $plain;
} else {
    $ENC_KEY = null;   // legacy plaintext client
}

$action = $req['action'] ?? '';
$db = is_array($req['db'] ?? null) ? $req['db'] : [];
$host = (string)($db['host'] ?? 'localhost');
$port = (int)($db['port'] ?? 3306);
$user = (string)($db['user'] ?? '');
$pass = (string)($db['pass'] ?? '');
$name = (string)($db['name'] ?? '');
$limit = (int)($req['limit'] ?? -1);
$timeout = max(5, min(3600, (int)($req['timeout'] ?? 300)));

if (!in_array($action, ['ping', 'query'], true)) fail(400, -1, 'Unknown action');
if (!$config['allow_remote_host'] && !in_array(strtolower($host), ['localhost', '127.0.0.1', '::1'], true))
    fail(403, -1, 'Remote database hosts are disabled on this agent (ALLOW_REMOTE_HOST)');
if ($port < 1 || $port > 65535) fail(400, -1, 'Invalid port');

set_time_limit($timeout + 10);
$m = mysqli_init();
$m->options(MYSQLI_OPT_CONNECT_TIMEOUT, 10);
$m->options(MYSQLI_OPT_READ_TIMEOUT, $timeout);
if (!@$m->real_connect($host, $user, $pass, $name, $port)) fail(200, mysqli_connect_errno(), mysqli_connect_error());
if (!$m->set_charset('utf8mb4')) $m->set_charset('utf8');

if ($action === 'ping') out(['ok' => true, 'version' => $m->server_info, 'threadId' => $m->thread_id]);

$sql = (string)($req['sql'] ?? '');
$m->query($limit >= 0 ? 'SET SESSION sql_select_limit=' . ($limit + 1) : 'SET SESSION sql_select_limit=DEFAULT');

$res = $m->query($sql);
if ($res === false) fail(200, $m->errno, $m->error);
if ($res === true) out(['ok' => true, 'columns' => [], 'rows' => [], 'affected' => $m->affected_rows,
                        'insertId' => $m->insert_id, 'truncated' => false]);

$cols = [];
$fields = $res->fetch_fields();
foreach ($fields as $f) $cols[] = ['name' => $f->name, 'type' => typeName($f->type), 'binary' => isBinaryField($f)];
$rows = [];
while ($row = $res->fetch_row()) $rows[] = array_map('cell', $row);
$res->free();
$truncated = $limit >= 0 && count($rows) > $limit;
if ($truncated) $rows = array_slice($rows, 0, $limit);
out(['ok' => true, 'columns' => $cols, 'rows' => $rows, 'affected' => 0, 'insertId' => 0, 'truncated' => $truncated]);
