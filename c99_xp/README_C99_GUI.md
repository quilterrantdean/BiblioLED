# BiblioLED C99 GUI (Windows XP 32-bit)

Este directório contém a migração C99 com foco em GUI Win32 nativa.

## O que já está implementado
- Core C99 com SQLite (CRUD e métricas).
- Janela Win32 nativa (`win_gui.c`) para teste local dos developers.
- Lista de expirações, estado de disponibilidade, botões de actualizar e novo registo (MVP).

## Build (MinGW/TDM-GCC/w64devkit)

### GUI
```bash
make gui
```
Gera `biblioled_gui.exe`.

### CLI (apenas suporte interno)
```bash
make release
```
Gera `biblioled_cli.exe`.

## Nota
A GUI é MVP para validação técnica e integração; os diálogos completos (novo/editar/histórico) serão refinados na fase seguinte.
