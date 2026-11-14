
import sqlite3

db = sqlite3.connect("database/project.db")

db.execute("PRAGMA foreign_keys = OFF")

tables = db.execute("""
    SELECT name
    FROM sqlite_master
    WHERE type = 'table'
      AND name NOT LIKE 'sqlite_%'
""").fetchall()

for (table,) in tables:
    db.execute('DELETE FROM "' + table.replace('"', '""') + '"')
    print("Cleared:", table)

db.commit()
db.close()

print("All application tables cleared.")
