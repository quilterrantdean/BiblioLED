# BiblioLED C99 GUI (Windows XP 32-bit)

## Estado actual
- Dashboard com expirações e disponibilidade.
- Novo registo com diálogo.
- Histórico completo em janela dedicada (`ListView`).
- Edição e eliminação de registos no histórico.
- Pesquisa por título no histórico (case-insensitive).
- Exportação CSV na GUI.
- Estatísticas resumidas na GUI (total, por tipo, primeiro/último registo).

## Build
```bash
make gui
```
ou
```bash
make gui-debug
```

## Nota para devs
Neste ambiente de chat não existe toolchain Win32, por isso a compilação deve ser validada localmente em Windows (Dev-C++, MinGW32/TDM-GCC-32, w64devkit).
