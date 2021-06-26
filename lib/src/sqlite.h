#include <sqlite3.h>
#include <stddef.h>

sqlite3 *open_db();

void setup_queries(const char *);

void create_table(sqlite3 *, const char *);

void create_row(sqlite3 *, const char *);

void add_command(sqlite3 *, const char *, const void *, size_t);
void add_output(sqlite3 *, const char *, const void *);
void add_start_time(sqlite3 *, const char *);
void add_finish_time(sqlite3 *, const char *);
void add_indicator(sqlite3 *, const char *);