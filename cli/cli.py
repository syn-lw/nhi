#!/usr/bin/python3
import sqlite3
import datetime
import subprocess
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

def get_log_str(logs, index):
    log = logs[index]
    command_line = f"{get_ps1(log)}{log['command'].decode()}"
    try:
        output = f"{command_line}\n{log['output'].decode()}"
    except:
        output = f"{command_line}\n{repr(log['output'])}"
    return output

def get_ps1(log):
    time = log["start_time"]
    time_log = datetime.datetime.fromtimestamp(time).strftime('%Y-%m-%d %H:%M:%S')
    return f"({time_log}) {log['PS1'].decode()}"

def pager_print(output):
    pager = subprocess.Popen(["less", "-R"], stdin=subprocess.PIPE)
    pager.communicate(input=output.encode())

def main():
    conn = sqlite3.connect('/var/nhi/db')

    conn.text_factory = bytes
    conn.row_factory = sqlite3.Row

    tables = get_tables(conn)
    logs = get_logs(conn, tables)
    commands = [(index, value["command"].decode()) for (index, value) in zip(range(len(logs)), logs)]
    commands = [f"{index} {command}" for (index, command) in commands]
    logs_selected = iterfzf(commands[::-1], multi=True)
    if logs_selected != None:
        logs_index = map(int, [selection.split(" ")[0] for selection in logs_selected])
        logs_index = sorted(logs_index)

        full_output = "\n\n".join([get_log_str(logs, i) for i in logs_index])
        pager_print(full_output)

if __name__ == '__main__':
    main()
