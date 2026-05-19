# Plano de Implementação M1 — Migração para C (Windows XP 32-bit)

## Objectivo de M1

Entregar um **núcleo funcional em C99** (sem GUI final) que replique a lógica crítica do projecto Python actual:

- inicialização e acesso a SQLite;
- CRUD de empréstimos;
- cálculo de métricas (limite diário/mensal, expirações, vagas);
- exportação CSV;
- leitura/escrita de configuração do utilizador.

Nesta fase, a validação será feita por um executável de consola (harness), para garantir equivalência de comportamento antes de portar a interface gráfica.

---

## Alvo técnico e compatibilidade

- **SO alvo:** Windows XP SP3 32-bit.
- **Compiladores:** MinGW32/TDM-GCC-32/w64devkit.
- **Linguagem:** C99 (`-std=c99`).
- **Dependências externas (mínimas):**
  - `sqlite3` (obrigatória);
  - `cJSON` (opcional, apenas para `user_settings.json`; pode ser substituída por INI no futuro).

---

## Estrutura de pastas proposta

```text
/include
  biblio_types.h
  config_store.h
  datetime_utils.h
  loans_repo.h
  metrics.h
  csv_export.h

/src
  main_cli.c
  config_store.c
  datetime_utils.c
  loans_repo.c
  metrics.c
  csv_export.c

/tests
  golden/
  test_cli_cases.txt

/third_party
  /sqlite3
  /cjson (opcional)
```

---

## Contratos de dados (API interna)

## 1) Tipos base (`biblio_types.h`)

- `loan_record_t`
  - `int id;`
  - `char title[256];`
  - `char loan_date[20];`  // `YYYY-MM-DD HH:MM`
  - `char type[32];`

- `dashboard_metrics_t`
  - `int daily_available;`
  - `int monthly_available;`
  - `int expiries_count;`
  - `expiry_item_t expiries[256];`
  - `int monthly_loans_count;`
  - `loan_record_t monthly_loans[1024];`

- `app_config_t`
  - `char db_file[260];`
  - `int loan_duration_days;`
  - `int monthly_window_days;`
  - `int max_daily_loans;`
  - `int max_monthly_loans;`
  - `char csv_delimiter;`

## 2) Erros

- Convenção uniforme:
  - `0` = sucesso;
  - `!=0` = erro.
- Funções devolvem código e preenchem `char *errbuf` opcional.

---

## Módulos e funções de M1

## A) Configuração (`config_store.*`)

- `void config_set_defaults(app_config_t *cfg);`
- `int config_load_user_settings(const char *path, app_config_t *cfg, char *err, size_t err_sz);`
- `int config_save_user_settings(const char *path, const app_config_t *cfg, char *err, size_t err_sz);`

Notas:
- Preservar defaults actuais: `biblioteca.db`, 21 dias, 30 dias, 5/15, `;`.
- Tolerar ficheiro ausente (não é erro fatal).

## B) Datas e parsing (`datetime_utils.*`)

- `int parse_db_datetime(const char *s, struct tm *out_tm);`
- `int format_db_datetime(const struct tm *tmv, char out[20]);`
- `int days_between(time_t a, time_t b);`

Notas:
- Formato canónico: `YYYY-MM-DD HH:MM`.
- Evitar dependências não-portáveis.

## C) Repositório SQLite (`loans_repo.*`)

- `int loans_init_db(const app_config_t *cfg, char *err, size_t err_sz);`
- `int loans_add(const app_config_t *cfg, const char *title, const char *loan_date, const char *type, char *err, size_t err_sz);`
- `int loans_delete(const app_config_t *cfg, int record_id, char *err, size_t err_sz);`
- `int loans_edit(const app_config_t *cfg, int record_id, const char *new_title, const char *new_date, const char *new_type, char *err, size_t err_sz);`
- `int loans_read_all(const app_config_t *cfg, loan_record_t *out, int out_cap, int *out_count, char *err, size_t err_sz);`

Notas:
- SQL alinhado com esquema actual (`loans: id,title,loan_date,type`).
- Queries parametrizadas para segurança.

## D) Métricas (`metrics.*`)

- `int metrics_get_dashboard(const app_config_t *cfg, dashboard_metrics_t *out, char *err, size_t err_sz);`
- `int metrics_find_next_monthly_slots(const app_config_t *cfg, const loan_record_t *monthly_loans, int count, slot_item_t *out_slots, int out_cap, int *out_count);`

Notas:
- Reproduzir exactamente a lógica actual de:
  - janela diária desde 00:00;
  - janela mensal em `monthly_window_days`;
  - expiração por `loan_duration_days`.

## E) CSV (`csv_export.*`)

- `int csv_export_history(const app_config_t *cfg, const loan_record_t *items, int count, const char *out_path, char *err, size_t err_sz);`

Notas:
- Cabeçalho: `Title;LoanDate;Type`.
- Delimitador configurável.

---

## Executável de validação (CLI)

`main_cli.c` deve suportar comandos:

- `init-db`
- `add --title ... --date "YYYY-MM-DD HH:MM" --type ...`
- `list`
- `metrics`
- `export --out ...`
- `delete --id ...`
- `edit --id ...`

Objectivo: permitir testes automáticos e comparação com resultados do Python.

---

## Testes de equivalência (golden)

Criar cenários controlados:

1. Base vazia.
2. Inserções no mesmo dia para testar limite diário.
3. Inserções ao longo de >30 dias para janela mensal.
4. Registos perto da fronteira de expiração (21 dias).
5. Exportação CSV e comparação byte a byte (excepto newline normalizado).

Critérios de aceitação de M1:

- contagens diárias/mensais iguais ao Python;
- lista de expirações com mesmas datas e ordem;
- slots mensais com mesmas datas de libertação;
- CRUD persistente sem corrupção;
- exportação CSV compatível.

---

## Makefile mínimo (XP-friendly)

Perfis:

- `debug`: `-m32 -std=c99 -O0 -g -Wall -Wextra`
- `release`: `-m32 -std=c99 -O2 -s -Wall -Wextra`

Link:

- `-lsqlite3`
- evitar dependências C++.

---

## Riscos e mitigação

1. **Diferenças de parsing temporal** entre Python e C.
   - Mitigar com testes de fronteira e formato fixo.
2. **Build de sqlite3 em XP/toolchains antigas.**
   - Fixar versão compatível e guardar binário/lib local.
3. **Encoding UTF-8 em títulos.**
   - Definir política explícita (UTF-8 interno; conversão só na GUI Win32 futura).

---

## Entregáveis de M1

1. Biblioteca C compilável (`core/data`).
2. Executável CLI de validação.
3. Testes golden e instruções de execução.
4. Documento de diferenças conhecidas (se existirem).

