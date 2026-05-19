#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "loans_repo.h"

static void set_err(char *err, size_t err_sz, const char *msg) {
    if (err && err_sz) {
        snprintf(err, err_sz, "%s", msg ? msg : "erro desconhecido");
    }
}

int loans_init_db(const app_config_t *cfg, char *err, size_t err_sz) {
    sqlite3 *db = NULL;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    const char *sql = "CREATE TABLE IF NOT EXISTS loans ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "title TEXT NOT NULL,"
                      "loan_date TEXT NOT NULL,"
                      "type TEXT NOT NULL)";
    if (sqlite3_open(db_file, &db) != SQLITE_OK) {
        set_err(err, err_sz, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return 1;
    }
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        set_err(err, err_sz, sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    sqlite3_close(db);
    set_err(err, err_sz, "");
    return 0;
}

int loans_add(const app_config_t *cfg, const char *title, const char *loan_date, const char *type, char *err, size_t err_sz) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    if (!title || !loan_date || !type) return 1;
    if (sqlite3_open(db_file, &db) != SQLITE_OK) {
        set_err(err, err_sz, sqlite3_errmsg(db)); if (db) sqlite3_close(db); return 1;
    }
    if (sqlite3_prepare_v2(db, "INSERT INTO loans (title, loan_date, type) VALUES (?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK) {
        set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_close(db); return 1;
    }
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, loan_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_finalize(stmt); sqlite3_close(db); return 1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    set_err(err, err_sz, "");
    return 0;
}

int loans_delete(const app_config_t *cfg, int record_id, char *err, size_t err_sz) {
    sqlite3 *db = NULL; sqlite3_stmt *stmt = NULL;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    if (sqlite3_open(db_file, &db) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); if (db) sqlite3_close(db); return 1; }
    if (sqlite3_prepare_v2(db, "DELETE FROM loans WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_close(db); return 1; }
    sqlite3_bind_int(stmt, 1, record_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) { set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_finalize(stmt); sqlite3_close(db); return 1; }
    sqlite3_finalize(stmt); sqlite3_close(db); set_err(err, err_sz, ""); return 0;
}

int loans_edit(const app_config_t *cfg, int record_id, const char *new_title, const char *new_date, const char *new_type, char *err, size_t err_sz) {
    sqlite3 *db = NULL; sqlite3_stmt *stmt = NULL;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    if (!new_title || !new_date || !new_type) return 1;
    if (sqlite3_open(db_file, &db) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); if (db) sqlite3_close(db); return 1; }
    if (sqlite3_prepare_v2(db, "UPDATE loans SET title=?, loan_date=?, type=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_close(db); return 1; }
    sqlite3_bind_text(stmt, 1, new_title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, new_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, new_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, record_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) { set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_finalize(stmt); sqlite3_close(db); return 1; }
    sqlite3_finalize(stmt); sqlite3_close(db); set_err(err, err_sz, ""); return 0;
}

int loans_read_all(const app_config_t *cfg, loan_record_t *out, int out_cap, int *out_count, char *err, size_t err_sz) {
    sqlite3 *db = NULL; sqlite3_stmt *stmt = NULL; int n = 0;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    if (out_count) *out_count = 0;
    if (sqlite3_open(db_file, &db) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); if (db) sqlite3_close(db); return 1; }
    if (sqlite3_prepare_v2(db, "SELECT id, title, loan_date, type FROM loans ORDER BY loan_date DESC, id DESC", -1, &stmt, NULL) != SQLITE_OK) { set_err(err, err_sz, sqlite3_errmsg(db)); sqlite3_close(db); return 1; }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (out && n < out_cap) {
            out[n].id = sqlite3_column_int(stmt, 0);
            snprintf(out[n].title, sizeof(out[n].title), "%s", (const char*)sqlite3_column_text(stmt, 1));
            snprintf(out[n].loan_date, sizeof(out[n].loan_date), "%s", (const char*)sqlite3_column_text(stmt, 2));
            snprintf(out[n].type, sizeof(out[n].type), "%s", (const char*)sqlite3_column_text(stmt, 3));
        }
        n++;
    }
    sqlite3_finalize(stmt); sqlite3_close(db);
    if (out_count) *out_count = n;
    set_err(err, err_sz, "");
    return 0;
}
