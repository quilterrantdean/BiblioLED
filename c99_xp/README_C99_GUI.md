# BiblioLED C99 GUI (Windows XP 32-bit)

Este directório contém a migração C99 com foco em GUI Win32 nativa.

## Estado actual (para testes de developers)
- Core C99 com SQLite (CRUD e métricas).
- GUI Win32 nativa funcional.
- Funcionalidades já disponíveis na GUI:
  - actualizar dashboard;
  - novo registo com diálogo dedicado (título/data-tipo);
  - histórico completo (MVP em janela de mensagem);
  - lista de expirações + barra de estado.

## Build (MinGW/TDM-GCC/w64devkit)

### GUI
```bash
make gui
```
Gera `biblioled_gui.exe`.

### GUI Debug
```bash
make gui-debug
```

### CLI (apenas suporte interno de engenharia)
```bash
make release
```

## Notas
- A GUI já é executável para teste local de integração.
- Próximo refinamento: histórico em janela dedicada com edição/eliminação directa e pesquisa.
