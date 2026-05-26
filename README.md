# FMVita

**FMVita** é um fork do **VitaShell** (TheFloW) com mudanças focadas em usabilidade, theme engine, background animado, sistema de Undo e reorganização completa dos dados para `ux0:FMVita/`.

## Diferenças do VitaShell original

### Novas features

#### Theme Engine & Interface
- **6 presets de cores** configuráveis: Dark, Light, Blue, Red, Purple, Brown — com cálculo automático de cores de texto, destaque, seleção, diálogos e bordas
- **Toolbar de ações rápidas** com 8 botões: Copiar, Colar, Excluir, Renomear, Filtrar, Agrupar, Pesquisar, Novo
- **Interface semi-transparente** quando usando background de imagem (GIF ou PNG) — topbar, cards e botões com alpha reduzido
- **Scrollbar temático** com cor de destaque e tamanho adaptativo
- **Barra de endereço** redesenhada com path + bateria + relógio
- **2 temas completos** embutidos: Default e Electron (novo)

#### Background Animado
- **4 animações procedurais**: Particles (ícones PS flutuantes), Waves (névoa colorida), Stars (campo estelar), Squares (retângulos)
- **GIF animado** como background (modo 4) — carregado de `ux0:FMVita/Gif/theme.gif` com fallbacks múltiplos
- **PNG estático** como background (modo 5) — carregado de `ux0:FMVita/Background/bg.png`
- Engine GIF baseada em stb_image com delays por frame, scaling cover-fit e clipping

#### Column View
- **Visualização em 3 colunas** (avô/pai/atual) para navegação rápida entre diretórios
- Coluna da esquerda mostra conteúdo do diretório avô, coluna do meio mostra o pai, direita mostra o atual
- Destaque em dourado para a pasta atual nas colunas pai/avô
- Toque em coluna navega para aquele diretório

#### Undo System
- Desfaz **Move** e **Copy** via contexto (`UNDO_ACTION`)
- Undo de Move reverte o `sceIoRename`
- Undo de Copy deleta o arquivo copiado
- Botão Undo aparece/desaparece dinamicamente no menu de contexto
- Drag-and-drop também registra undo

#### Sistema de Toque Avançado
- **Diálogo de confirmação touch** substitui o diálogo YESNO do sistema — botões Yes/No com tema e ícones de botão PS
- **Diálogo de exclusão touch** com confirmação
- **Diálogo FTP touch** com status do servidor e botões Conectar/Parar
- **Drag-and-drop** de arquivos: quadrado + toque ou pressão longa, arrasta para pasta
- **Auto-scroll** durante drag perto das bordas
- **Gestos de swipe**: esquerda = voltar, direita = entrar
- **Multi-tap**: duplo toque com segurada abre contexto
- **Mouse cursor via analógico direito**: ponteiro completo com clique e fade após inatividade

#### Navegação
- **Bookmarks**: atalho `Quadrado` pula para `ux0:FMVita/bookmarks/`
- **Arquivos Recentes**: atalho `Triângulo` pula para `ux0:FMVITA/recent/` — arquivos abertos geram symlink `.lnk` automaticamente
- **Filtro**: All/Folders/Files no toolbar
- **Pesquisa**: busca case-insensitive no diretório atual
- **Scroll infinito**: wrapping do topo para o fim e vice-versa
- **Scroll suave**: lerp-based com aceleração
- **Mouse cursor** com crosshair e clique por Cross ao posicionar sobre arquivos/botões
- **Rastreamento de symlinks**: `SymlinkDirectoryPath` linked list para navegação correta em atalhos

#### Segurança
- **Proteção de pastas do sistema**: rename/bloqueio com mensagem de erro customizada
- **isProtectedPath()** verifica diretórios críticos antes de operações destrutivas

#### Configuração
- `use_custom_config = 1` por padrão (pode desabilitar segurando L no boot)
- Config lida/escrita de `ux0:FMVita/settings.txt`
- Novas opções: USB Device (4 modos), SELECT Button (USB/FTP), View Mode (List/Grid/Column), Background Anim (6 modos), Theme Preset (6 cores)

### Features removidas do original
- **HENkaku Settings**: PSN spoofing, unsafe homebrew toggle, version spoofing — removidos do menu (código ainda existe na struct como inutilizado)
- **Disable Auto-Update**: campo no struct mas não aparece no menu
- **Disable Warning Messages**: campo no struct mas não aparece no menu
- **Enable Rear Touch Drag**: renomeado `_unused_rear_touchpad`, funcionalidade removida
- **Lixeira**: exclusão é sempre permanente (junto com sistema de trash)
- **GIF Overlay Alpha**: removido das configurações
- **Enable GIF BG**: removido (GIF controlado exclusivamente por `background_anim == 4`)

### Mudanças de paths
| Original VitaShell | FMVita |
|---|---|
| `ux0:VitaShell/settings.txt` | `ux0:FMVita/settings.txt` |
| `ux0:VitaShell/language/` | `ux0:FMVita/language/` |
| `ux0:VitaShell/theme/` | `ux0:FMVita/theme/` |
| `ux0:data/lastdir.txt` | `ux0:FMVita/internal/lastdir.txt` |
| `ux0:VitaShell/bookmarks/` | `ux0:FMVita/bookmarks/` |
| `ux0:data/recent/` | `ux0:FMVITA/recent/` |
| `ux0:app/VITASHELL/module/*` | `ux0:FMVita/module/*` |
| *não existia* | `ux0:FMVita/Gif/theme.gif` |
| *não existia* | `ux0:FMVita/Background/bg.png` |

### Módulos adicionados
- `modules/kernel/` — VitaShellKernel2 (carrega umass.skprx de `ux0:FMVita/module/`)
- `modules/patch/` — VitaShellPatch
- `modules/user/` — VitaShellUser
- `modules/usbdevice/` — driver USB device

### Arquivos novos no build
- `buttons.c`/`buttons.h` — renderização de ícones de botão PS
- `main_context.c` — contextos completamente reorganizados com submenus (Bookmarks, Adhoc, New)
- `stb_image.h` / `stb_image_resize2.h` — decodificação GIF
- `build/resources/image/theme.png` — PNG background padrão embutido

### Mudanças internas
- `main.h`: ~30 funções inline de cores de tema, externs de undo e touch confirm
- `theme.c`: engine GIF completo (load/update/draw/free), engine PNG, drawImageBackground()
- `browser.c`: column view, drag-and-drop, touch engine, mouse cursor, filter/search, scroll clipping fix, DPAD scroll fix (9→MAX_ENTRIES), infinite scroll
- `main.c`: touch confirm dialog, delete confirm dialog, FTP touch dialog, scrollbar temático, toolbar, is_img_bg transparency
- `settings.c`: 6 opções de background, 3 view modes, 6 theme presets, menu HENkaku removido
- `photo.c`: guard `>= 4` para não desenhar animações sobre background de imagem
- `init.c`: `default_files[]` com PNG, diretórios `Gif/` e `Background/`
- `vitashell_config.h`: view_mode, background_anim, theme_preset adicionados; rear_touchpad renomeado `_unused`
- `language.h`/`.c`: ~30 novas entradas (undo, filter, toolbar, FTP, bg_anim, view modes, presets)
- `context_menu.c`: todas as cores via theme helpers

### Tradução
- `english_us.txt`: ~27 novas entradas
- Arquivo padrão copiado para `ux0:FMVita/language/english_us.txt` na primeira execução

## Building

```bash
mkdir build && cd build && cmake .. && make
```

Requer [vitasdk](https://github.com/vitasdk) e os toolchains padrão.

## Créditos

- **TheFloW** — VitaShell original
- **Team Molecule** — HENkaku
- **xerpi** — ftpvitalib, vita2dlib
- **Sean Barrett** — stb_image
- **WolffsRoom** — modificações FMVita
