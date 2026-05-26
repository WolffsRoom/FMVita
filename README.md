<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/icon0.png" width="128" alt="Logo FMVita">
  <h1>FMVita (File Manager Vita)</h1>
</div>

**FMVita** é um fork aprimorado do **VitaShell** (criado por *TheFloW*). Esta versão foca  em **usabilidade**, expandindo o suporte à tela *touchscreen* do PS Vita, introduzindo *backgrounds* animados, diversas melhorias de Qualidade de Vida (QoL) e uma nova organização dos arquivos do programa  para o diretório independente `ux0:FMVita/`.

![Tela Inicial](https://github.com/WolffsRoom/FMVita/raw/master/media/Tela%20Inicial.png)

---

## Instalação

O FMVita foi projetado para evitar problemas com suas configurações atuais. Por isso ele possui um **Title ID exclusivo** e estrutura de pastas própria.

* **Zero Conflitos:** Pode ser instalado lado a lado com o VitaShell original, OneMenuPlus, MolecularShell ou qualquer outro gerenciador.
* **Como instalar:** Basta transferir o arquivo `.vpk` do FMVita para o console (via FTP ou USB) e instalá-lo usando o VitaShell (ou qualquer outro aplicativo gerenciador de arquivos que você já possua).

---

## Principais Diferenças e Novas Funcionalidades

### Interface e Temas

![Modo Grid](https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Grid.png)

* **Seis Presets de Cores:** *Dark, Light, Blue, Red, Purple* e *Brown*. O sistema calcula automaticamente as cores de texto, destaque, seleção, diálogos e bordas.
* **Menu de ação rápida:** Nova barra de ferramentas com 8 atalhos rápidos: *Copiar, Colar, Excluir, Renomear, Filtrar, Agrupar, Pesquisar* e *Novo*.
* **Transparência:** Interface (topbar, cards e botões) com canal alpha reduzido ao utilizar imagens de fundo (GIF ou PNG) para melhor visualização.
* **Scrollbar Temático:** Barra de rolagem adaptativa que acompanha a cor de destaque do tema.
* **Barra de Endereço Redesenhada:** Agora exibe o caminho atual (*path*), status da bateria e relógio.
* **Suporte parcial a temas VitaShell:** Dado a natureza do projeto original, os temas desenvolvidos para ele não são 100% compatíveis nessa versão, aproveitando apenas alguns elementos como os ícones de arquivos. Dado a essa limitação, desenvolvi o tópico a seguir.

### Backgrounds Animados
* **Quatro Animações Procedurais:** *Particles* (ícones PlayStation flutuantes), *Waves* (névoa colorida), *Stars* (campo estelar) e *Squares* (retângulos com estética PS2).
* **Suporte a GIF Animado:** Carregamento de `ux0:FMVita/Gif/theme.gif` (Modo 4) com *fallbacks* múltiplos. Motor baseado em `stb_image` com controle de atraso por frame, dimensionamento *cover-fit* e *clipping*.
* **Suporte a PNG Estático:** Carregamento nativo de `ux0:FMVita/Background/bg.png` (Modo 5).

### Suporte a Touch Screen

![Servidor FTP](https://github.com/WolffsRoom/FMVita/raw/master/media/FTP%20Server.png)

* **Diálogos Nativos:** O antigo sistema YES/NO foi substituído por **Touch Dialogs** (Confirmação, Exclusão e FTP) estilizados com os ícones dos botões PlayStation.
* **Drag-and-Drop (Arrastar e Soltar):** Segure `Quadrado` + Toque (ou apenas pressione longamente) para arrastar arquivos para outras pastas. Inclui *Auto-scroll* ao aproximar o item das bordas.
* **Swipe Gestures:** Deslize para a Esquerda para *Voltar*, Direita para *Entrar*.
* **Multi-tap:** Toque duplo segurado para abrir o menu de contexto.
* **Mouse Cursor:** Controle um ponteiro de mouse com o analógico direito (clique com `Cross`), com efeito de *fade* após inatividade.

### Navegação e Visualização

![Modo Coluna 1](https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Coluna.png)
![Modo Coluna 2](https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Coluna2.png)

* **Column View:** Inovadora visualização em 3 colunas (*Avô / Pai / Atual*) para navegação ultrarrápida. Destaque em dourado para a pasta ativa nas colunas superiores e navegação por toque direto na coluna.
* **Bookmarks e Recentes:** Atalho `Quadrado` pula para os favoritos; Atalho `Triângulo` vai para arquivos recentes (que geram *symlinks* `.lnk` automaticamente).
* **Ferramentas de Busca:** Sistema de Filtro (All/Folders/Files) na toolbar e Pesquisa *case-insensitive* no diretório atual.
* **Scroll Aprimorado:** Scroll infinito (*wrapping* do topo para o fim) e rolagem suave (baseada em *lerp* com aceleração).
* **Symlinks:** Rastreamento otimizado via lista encadeada `SymlinkDirectoryPath` para navegação correta em atalhos.

### Sistema de Refazer (Undo)
* Desfaça ações de **Move** e **Copy** diretamente pelo menu de contexto (`UNDO_ACTION`).
* *Undo de Move:* Reverte o comando `sceIoRename`.
* *Undo de Copy:* Deleta de forma limpa o arquivo copiado por engano.
* O botão de *Undo* aparece dinamicamente e também registra ações feitas por *Drag-and-Drop*.

### Segurança e Configuração

![Tela de Configuração](https://github.com/WolffsRoom/FMVita/raw/master/media/Tela%20de%20Config.png)

* **Proteção de Sistema:** Bloqueio de renomeação em pastas críticas do sistema (verificadas via `isProtectedPath()`) com mensagens de erro customizadas.
* **Configurações Independentes:** Lê e escreve diretamente em `ux0:FMVita/settings.txt` (`use_custom_config = 1` por padrão; pode ser desativado segurando `L` no boot).
* **Novas Opções no Menu:** *USB Device* (4 modos), *SELECT Button* (USB/FTP), *View Mode* (List/Grid/Column), *Background Anim* (6 modos) e *Theme Preset* (6 cores).

---

## Recursos Removidos e Otimizações
Para manter o aplicativo leve e focado no gerenciamento de arquivos, os seguintes recursos do VitaShell original foram descontinuados/ocultados:
* Menu **HENkaku Settings** (PSN spoofing, unsafe homebrew toggle, version spoofing).
* *Disable Auto-Update* e *Disable Warning Messages*.
* Suporte ao *Rear Touchpad*.

---

## Mudanças de Estrutura de Diretórios

O FMVita isola todos os seus dados do VitaShell original:

| Original (VitaShell) | FMVita |
| :--- | :--- |
| `ux0:VitaShell/settings.txt` | `ux0:FMVita/settings.txt` |
| `ux0:VitaShell/language/` | `ux0:FMVita/language/` |
| `ux0:VitaShell/theme/` | `ux0:FMVita/theme/` |
| `ux0:data/lastdir.txt` | `ux0:FMVita/internal/lastdir.txt` |
| `ux0:VitaShell/bookmarks/` | `ux0:FMVita/bookmarks/` |
| `ux0:data/recent/` | `ux0:FMVITA/recent/` |
| `ux0:app/VITASHELL/module/*` | `ux0:FMVita/module/*` |
| *(Não existia)* | `ux0:FMVita/Gif/theme.gif` |
| *(Não existia)* | `ux0:FMVita/Background/bg.png` |

---

## Detalhes Técnicos e Build

### Modificações no Código-Fonte
* **Novos Módulos:** `modules/kernel/` (carrega `umass.skprx` próprio), `modules/patch/`, `modules/user/` e driver `usbdevice/`.
* **Novos Arquivos:** Implementação de `buttons.c/.h` para ícones do PS, `main_context.c` para submenus organizados e engine decodificadora com `stb_image.h`.
* **Tradução (`english_us.txt`):** Mais de 27 novas entradas integradas e copiadas para o diretório nativo na primeira execução.
* **Refatoração Interna:** Mais de 30 funções inline de cores no `main.h`, reestruturação do `browser.c` (D-PAD scroll fix, column view, touch engine) e limpeza de opções antigas no `settings.c`.

### Instruções de Build
Requer [vitasdk](https://github.com/vitasdk) e os toolchains padrão instalados e configurados.

```bash
mkdir build
cd build
cmake ..
make
```

## Detalhes Técnicos e BuildCréditos
* **TheFloW** — Criador do VitaShell original.
* **Team Molecule** — HENkaku.
* **xerpi** — Bibliotecas ftpvitalib e vita2dlib.
* **Sean Barrett** — Implementação do stb_image.
* **WolffsRoom** — Desenvolvimento, modificações visuais e engine do FMVita.
