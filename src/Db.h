#pragma once
#include <QString>

namespace Db {
// Opens the default SQLite connection (":memory:" allowed) and runs migrations. GUI thread only.
bool open(const QString &path);
QString get(const QString &key, const QString &def = {});
void set(const QString &key, const QString &value);
}
