# data_utils.py
# -*- coding: utf-8 -*-
# Utilitários para gestão de dados via SQLite e Análise Estatística

import csv
import datetime
import os
import sqlite3
import shutil

import config

DB_FILE = config.DB_FILE
CSV_LEGACY = config.HISTORY_FILE_CSV
DATETIME_FORMAT = config.DATETIME_FORMAT
DATE_ONLY_FORMAT = config.DATE_ONLY_FORMAT

def init_db():
    db_exists = os.path.exists(DB_FILE)
    conn = None
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS loans (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                loan_date TEXT NOT NULL,
                type TEXT NOT NULL
            )
        ''')
        conn.commit()
    except Exception as e:
        return "Erro ao inicializar BD: {}".format(e)
    finally:
        if conn: conn.close()

    if not db_exists and os.path.exists(CSV_LEGACY):
        return _migrate_csv_to_sqlite()
    return None

def _migrate_csv_to_sqlite():
    try:
        records = []
        with open(CSV_LEGACY, 'r', encoding='utf-8', newline='') as csvfile:
            reader = csv.DictReader(csvfile, delimiter=config.CSV_DELIMITER)
            for row in reader:
                title = (row.get('Title', '') or '').strip()
                date_str = row.get('LoanDate', '').strip()
                l_type = (row.get('Type', '') or '').strip().lower()
                if title and date_str:
                    try:
                        dt = datetime.datetime.strptime(date_str, DATE_ONLY_FORMAT)
                        new_date_str = dt.strftime(DATETIME_FORMAT)
                        records.append((title, new_date_str, l_type))
                    except ValueError:
                        continue 
        if records:
            conn = sqlite3.connect(DB_FILE)
            cursor = conn.cursor()
            cursor.executemany('INSERT INTO loans (title, loan_date, type) VALUES (?, ?, ?)', records)
            conn.commit()
            conn.close()
        shutil.move(CSV_LEGACY, CSV_LEGACY + ".bak")
        return "Migração de CSV para Base de Dados concluída. Cópia de segurança criada."
    except Exception as e:
        return "Erro na migração de dados: {}".format(e)

def parse_db_datetime(date_str):
    try:
        return datetime.datetime.strptime(date_str, DATETIME_FORMAT)
    except Exception:
        return None

def read_history():
    history = []
    if not os.path.exists(DB_FILE): return []
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute("SELECT id, title, loan_date, type FROM loans ORDER BY loan_date DESC, id DESC")
        rows = cursor.fetchall()
        conn.close()
        for r in rows:
            rid, title, d_str, l_type = r
            dt_obj = parse_db_datetime(d_str)
            if dt_obj:
                history.append({'id': rid, 'title': title, 'date': dt_obj, 'type': l_type})
    except Exception as e:
        return "Erro de Leitura da BD: {}".format(e)
    return history

def add_new_record_to_history(title, full_datetime_obj, loan_type):
    try:
        date_str = full_datetime_obj.strftime(DATETIME_FORMAT)
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute("INSERT INTO loans (title, loan_date, type) VALUES (?, ?, ?)", (title, date_str, loan_type.lower()))
        conn.commit()
        conn.close()
        return True, "Novo registo adicionado."
    except Exception as e:
        return False, "Erro ao adicionar: {}".format(e)

def delete_record(record_id):
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute("DELETE FROM loans WHERE id = ?", (record_id,))
        if cursor.rowcount == 0:
            conn.close()
            return False, "Registo não encontrado."
        conn.commit()
        conn.close()
        return True, "Registo eliminado com sucesso."
    except Exception as e:
        return False, "Erro ao eliminar: {}".format(e)

def edit_record(record_id, new_title, new_date_obj, new_type):
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute("SELECT loan_date FROM loans WHERE id = ?", (record_id,))
        row = cursor.fetchone()
        if not row:
            conn.close()
            return False, "Registo original não encontrado."
        
        old_dt = parse_db_datetime(row[0])
        
        if isinstance(new_date_obj, datetime.datetime):
            final_dt = new_date_obj
        else:
            old_time = old_dt.time() if old_dt else datetime.time(0, 0)
            final_dt = datetime.datetime.combine(new_date_obj, old_time)

        new_date_str = final_dt.strftime(DATETIME_FORMAT)
        cursor.execute("UPDATE loans SET title=?, loan_date=?, type=? WHERE id=?", (new_title, new_date_str, new_type.lower(), record_id))
        conn.commit()
        conn.close()
        return True, "Registo editado com sucesso."
    except Exception as e:
        return False, "Erro ao editar: {}".format(e)

def get_dashboard_metrics():
    now = datetime.datetime.now()
    today_start = datetime.datetime.combine(now.date(), datetime.time(0, 0, 0))
    window_start = now - datetime.timedelta(days=config.MONTHLY_WINDOW_DAYS)
    expiry_threshold = now - datetime.timedelta(days=config.LOAN_DURATION_DAYS)

    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()

    cursor.execute("SELECT COUNT(*) FROM loans WHERE loan_date >= ? AND loan_date <= ?", 
                   (today_start.strftime(DATETIME_FORMAT), now.strftime(DATETIME_FORMAT)))
    daily_count = cursor.fetchone()[0]

    cursor.execute("SELECT id, title, loan_date, type FROM loans WHERE loan_date >= ? AND loan_date <= ? ORDER BY loan_date ASC",
                   (window_start.strftime(DATETIME_FORMAT), now.strftime(DATETIME_FORMAT)))
    monthly_rows = cursor.fetchall()
    monthly_loans = [{'id': r[0], 'title': r[1], 'date': parse_db_datetime(r[2]), 'type': r[3]} for r in monthly_rows]

    cursor.execute("SELECT id, title, loan_date, type FROM loans WHERE loan_date >= ?",
                   (expiry_threshold.strftime(DATETIME_FORMAT),))
    expiry_rows = cursor.fetchall()
    conn.close()

    expiries = []
    loan_duration_td = datetime.timedelta(days=config.LOAN_DURATION_DAYS)
    for r in expiry_rows:
        dt = parse_db_datetime(r[2])
        if dt:
            expiry_date = dt + loan_duration_td
            if expiry_date >= now:
                expiries.append({
                    'title': r[1],
                    'expiry_date': expiry_date,
                    'days_left': (expiry_date - now).days,
                    'type': r[3].capitalize()
                })
    
    expiries.sort(key=lambda x: x['expiry_date'])
    daily_available = max(0, config.MAX_DAILY_LOANS - daily_count)
    monthly_available = max(0, config.MAX_MONTHLY_LOANS - len(monthly_loans))

    return {
        'daily_available': daily_available,
        'monthly_available': monthly_available,
        'monthly_loans_list': monthly_loans,
        'expiries': expiries
    }

def find_next_monthly_slot_dates(monthly_loans_list):
    if not monthly_loans_list: 
        return []
    
    release_dates = []
    window_delta = datetime.timedelta(days=config.MONTHLY_WINDOW_DAYS)
    now = datetime.datetime.now()
    
    for loan in monthly_loans_list:
        release_dt = loan['date'] + window_delta
        if release_dt > now:
            release_dates.append({
                'dt': release_dt, 
                'trigger_title': loan['title']
            })
    
    release_dates.sort(key=lambda x: x['dt'])
    
    result = []
    for item in release_dates:
        result.append("{} ({})".format(
            item['dt'].strftime(DATETIME_FORMAT), 
            item['trigger_title']
        ))
    
    return result

def export_history_to_csv(history_data, out_filepath):
    try:
        with open(out_filepath, 'w', encoding='utf-8', newline='') as fout:
            writer = csv.writer(fout, delimiter=config.CSV_DELIMITER)
            writer.writerow(config.CSV_HEADER)
            for item in history_data:
                writer.writerow([
                    item['title'], 
                    item['date'].strftime(DATETIME_FORMAT), 
                    item['type']
                ])
        return True, "Exportação concluída com sucesso: {}".format(out_filepath)
    except Exception as e:
        return False, "Erro ao exportar: {}".format(e)

def get_advanced_stats():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    stats = {}

    cursor.execute("SELECT count(*), min(loan_date), max(loan_date) FROM loans")
    res = cursor.fetchone()
    stats['total_loans'] = res[0] or 0
    stats['first_loan'] = res[1]
    stats['last_loan'] = res[2]

    cursor.execute("SELECT type, count(*) FROM loans GROUP BY type")
    stats['types'] = dict(cursor.fetchall())

    cursor.execute("SELECT strftime('%Y', loan_date), count(*) FROM loans GROUP BY 1 ORDER BY 1")
    stats['annual'] = dict(cursor.fetchall())

    cursor.execute("SELECT strftime('%Y-%m-%d', loan_date), count(*) FROM loans GROUP BY 1")
    stats['daily_activity'] = dict(cursor.fetchall())

    cursor.execute("SELECT DISTINCT date(loan_date) FROM loans ORDER BY loan_date")
    dates_str = [r[0] for r in cursor.fetchall() if r[0] ]
    
    longest_streak = 0
    if dates_str:
        dates = [datetime.datetime.strptime(d, '%Y-%m-%d').date() for d in dates_str]
        longest_streak = 1
        current_streak = 1
        for i in range(1, len(dates)):
            delta = (dates[i] - dates[i-1]).days
            if delta == 1:
                current_streak += 1
            else:
                if current_streak > longest_streak:
                    longest_streak = current_streak
                current_streak = 1
        if current_streak > longest_streak:
            longest_streak = current_streak
    stats['longest_streak'] = longest_streak
    
    if len(dates_str) > 1:
        first = datetime.datetime.strptime(dates_str[0], '%Y-%m-%d')
        last = datetime.datetime.strptime(dates_str[-1], '%Y-%m-%d')
        days_diff = (last - first).days
        stats['avg_days_between'] = round(days_diff / len(dates_str), 1)
    else:
        stats['avg_days_between'] = 0

    conn.close()
    return stats

def get_activity_calendar_data(year_str):
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    start_date = "{}-01-01 00:00:00".format(year_str)
    end_date = "{}-12-31 23:59:59".format(year_str)
    
    cursor.execute(
        "SELECT substr(loan_date, 1, 10), count(*) FROM loans WHERE loan_date BETWEEN ? AND ? GROUP BY 1", 
        (start_date, end_date)
    )
    
    data = {}
    for r in cursor.fetchall():
        dt = datetime.datetime.strptime(r[0], "%Y-%m-%d").date()
        data[dt] = r[1]
    
    conn.close()
    return data