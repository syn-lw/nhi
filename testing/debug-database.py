import sqlite3

def connect_to_database():
    conn = sqlite3.connect('/var/nhi/db')
    cursor = conn.cursor()
    conn.text_factory = bytes
    return cursor

def get_tables(cursor):
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
    return cursor.fetchall()

def dump_table(cursor, table):
    cursor.execute(f"SELECT command, output FROM '{table}'")
    for result in cursor.fetchall():
        if result[0] != None:
            if 'python3 debug-database.py' not in result[0].decode():
                print(result[0].decode())
                print(result[1])

def main():
    cursor = connect_to_database()
    tables = get_tables(cursor)
    dump_table(cursor, tables[-1][0].decode())

if __name__ == '__main__':
    main()
