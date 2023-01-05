#!/usr/bin/python3
import sqlite3
import datetime
from iterfzf import iterfzf

def get_tables(conn):
    tables_query = "SELECT name FROM sqlite_master WHERE type='table' AND name!='meta'"
    cursor = conn.cursor()
    cursor.execute(tables_query)
    #return cursor.fetchall()
    return [result["name"].decode() for result in cursor.fetchall()]

def get_logs(conn, tables):
    assert all(table.isnumeric() for table in tables), "table name not numeric"
    list_of_logs = []
    for table in tables:
        logs_query = f'SELECT * FROM "{table}"'
        cursor = conn.cursor()
        cursor.execute(logs_query)
        list_of_logs.extend(cursor.fetchall())
    return list(filter(lambda x: x["command"] != None, list_of_logs))

def print_log(logs, index):
    log = logs[index]
    command_line = f"{get_ps1(log)}{log['command'].decode()}"
    print(command_line)
    #print(log["output"])
    try:
        print(log["output"].decode())
    except:
        print(log["output"])

def get_ps1(log):
    time = log["start_time"]
    time_log = datetime.datetime.fromtimestamp(time).strftime('%Y-%m-%d %H:%M:%S')
    return f"({time_log}) {log['PS1'].decode()}"

def main():
    conn = sqlite3.connect('/var/nhi/db')

    conn.text_factory = bytes
    conn.row_factory = sqlite3.Row

    tables = get_tables(conn)
    logs = get_logs(conn, tables)
    commands = [(index, value["command"].decode()) for (index, value) in zip(range(len(logs)), logs)]
    commands = [f"{index} {command}" for (index, command) in commands]
    index = int(iterfzf(commands).split(" ")[0])

    print_log(logs, index)

if __name__ == '__main__':
    main()
