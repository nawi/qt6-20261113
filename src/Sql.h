#pragma once
#include <QRegularExpression>
#include <QVariant>
#include <QString>
#include <QStringList>

// SQL text helpers shared by the grid, filter builder and SQL editor guards.
// Identifiers are always backtick-quoted; values are always escaped as string literals.
namespace Sql {

inline QString quoteId(const QString &name)
{
    QString n = name;
    n.replace('`', "``");
    return '`' + n + '`';
}

inline QString literal(const QVariant &v, bool isNull = false)
{
    if (isNull || v.isNull()) return QStringLiteral("NULL");
    QString s = v.toString();
    s.replace('\\', "\\\\").replace('\'', "\\'");
    return '\'' + s + '\'';
}

inline QString qualified(const QString &db, const QString &name)
{
    return quoteId(db) + '.' + quoteId(name);
}

// SQL editor guards from PLAN: no DROP/TRUNCATE; UPDATE/DELETE require WHERE.
inline QString guardWrite(const QString &sql)
{
    const QString s = sql.trimmed();
    const QString upper = s.toUpper();
    auto startsWord = [&](const char *w) {
        const QString word = QString::fromLatin1(w);
        return upper.startsWith(word)
               && (upper.size() == word.size() || !upper.at(word.size()).isLetterOrNumber());
    };
    if (startsWord("DROP") || startsWord("TRUNCATE"))
        return QStringLiteral("DROP and TRUNCATE are blocked in the SQL editor.");
    if (startsWord("UPDATE") || startsWord("DELETE")) {
        if (!QRegularExpression(QStringLiteral("\\bWHERE\\b"),
                                QRegularExpression::CaseInsensitiveOption).match(s).hasMatch())
            return QStringLiteral("UPDATE and DELETE require a WHERE clause.");
    }
    return {};
}

inline QString filterWhere(const QVariantList &filters)
{
    QStringList parts;
    for (int i = 0; i < filters.size(); ++i) {
        const QVariantMap f = filters[i].toMap();
        const QString col = f.value("column").toString();
        const QString op = f.value("op").toString().toUpper();
        const QVariant val = f.value("value");
        if (col.isEmpty()) continue;
        QString expr;
        if (op == "IS NULL") expr = quoteId(col) + " IS NULL";
        else if (op == "IS NOT NULL") expr = quoteId(col) + " IS NOT NULL";
        else if (op == "IN") {
            QStringList vals;
            const QStringList list = val.toString().split(',', Qt::SkipEmptyParts);
            for (const QString &v : list) vals << literal(v.trimmed());
            if (vals.isEmpty()) continue;
            expr = quoteId(col) + " IN (" + vals.join(',') + ')';
        } else {
            static const QStringList allowed{"=", "!=", "<>", "<", ">", "<=", ">=", "LIKE", "NOT LIKE"};
            if (!allowed.contains(op)) continue;
            expr = quoteId(col) + ' ' + op + ' ' + literal(val);
        }
        const QString join = f.value("join").toString().toUpper() == "OR" ? " OR " : " AND ";
        parts << (parts.isEmpty() ? expr : join + expr);
    }
    return parts.isEmpty() ? QString() : " WHERE " + parts.join(QString());
}

// WHERE clause from filters AND a free-text search over `columns` (CAST to CHAR, LIKE %text%).
inline QString whereClause(const QVariantList &filters, const QString &search, const QStringList &columns)
{
    const QString base = filterWhere(filters);   // " WHERE ..." or ""
    QString text = search;
    if (text.isEmpty() || columns.isEmpty()) return base;
    text.replace('\\', "\\\\").replace('%', "\\%").replace('_', "\\_");
    const QString lit = literal('%' + text + '%');
    QStringList ors;
    for (const QString &c : columns) ors << "CAST(" + quoteId(c) + " AS CHAR) LIKE " + lit;
    const QString s = '(' + ors.join(" OR ") + ')';
    if (base.isEmpty()) return " WHERE " + s;
    return " WHERE (" + base.mid(7) + ") AND " + s;
}

} // namespace Sql
