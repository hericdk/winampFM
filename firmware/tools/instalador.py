#!/usr/bin/env python3
"""winampFM: the web installer goes into the launch page (site/index.html, the panel at the top).
    python3 firmware/tools/instalador.py [VERSION]
  -> release/winampfm-VERSION.fwsc, site/firmware/ (the package), site/editor/ (backups), and the installer's script injected into
     site/index.html between <!--INSTALLER--> and <!--/INSTALLER--> (FM1 Quest's web/make_site.py builds it; we take its script)."""
import glob, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
V = sys.argv[1] if len(sys.argv) > 1 else '0.1'
os.chdir(ROOT)
Path('release').mkdir(exist_ok=True)
pkg = f'release/winampfm-{V}.fwsc'
shutil.copy('firmware/build/felucca.fwsc', pkg)
tmp = Path(tempfile.mkdtemp())
subprocess.run([sys.executable, '../fm1-quest-github/web/make_site.py', pkg, V, str(tmp / 'o')], check=True, stdout=subprocess.DEVNULL)
src = (tmp / 'o/webapp/installer/index.html').read_text()
js = src[src.index('<script type="module">'):]
js = js[:js.index('</script>') + len('</script>')]
# the package next to the page, the editor beside it
js = re.sub(r'"pkg": "[^"]+"', f'"pkg": "firmware/winampfm-{V}.fwsc"', js)
shutil.rmtree('site/firmware', ignore_errors=True); Path('site/firmware').mkdir(parents=True)
shutil.copy(pkg, f'site/firmware/winampfm-{V}.fwsc')
shutil.rmtree('site/editor', ignore_errors=True)                  # (no editor link for now: a simpler one later)
# the texts: winampFM, what each step does, and Portuguese (the page's EN / PT switch)
R = [('FM1 Quest', 'winampFM'), ('start: "Starting", verify: "Verifying", loader: "Switching to update mode", write: "Writing",\n    reboot: "Restarting", done: "Done",',
      'start: "Talking to the FM-1…", verify: "Step 1 of 3 · sending winampFM to the FM-1 and checking it", loader: "Step 2 of 3 · the FM-1 restarts in update mode", write: "Step 3 of 3 · writing winampFM. Do not unplug the cable",\n    reboot: "Restarting the FM-1…", done: "Done! winampFM is installed: ",')]
for a, b in R:
    js = js.replace(a, b)
PT = '''  pt: {
    intro: "Ligue o FM-1 no USB e aperte Instalar. Só no Chrome ou no Edge (Web MIDI).", warn: "Não desconecte o cabo USB enquanto ele grava.",
    install: "Instalar o winampFM", editor: "Abrir o editor (backup, sons e sequências pelo computador)",
    direct: "Ligue o cabo USB direto no computador. Não precisa de probe nem de Transporter.",
    brick: "Se uma instalação falhar e o FM-1 não ligar mais, para recuperar é preciso um Transporter:",
    beta: "O winampFM é um firmware experimental e pode ser instável. Não foi testado em todas as unidades. Os autores não se responsabilizam por danos ou mau funcionamento do seu FM-1. Use por sua conta e faça backup antes.",
    nomidi: "Este navegador não consegue. Abra a página no Chrome ou no Edge.", loadfail: "Não carregou: ",
    start: "Conversando com o FM-1…", verify: "Passo 1 de 3 · enviando o winampFM ao FM-1 e conferindo", loader: "Passo 2 de 3 · o FM-1 reinicia em modo de atualização",
    write: "Passo 3 de 3 · gravando o winampFM. Não desconecte o cabo", reboot: "Reiniciando o FM-1…", done: "Pronto! O winampFM está instalado: ",
    stopped: "Parou. Reconecte o cabo USB e aperte Instalar de novo.", error: "Erro: ",
    denied: "O acesso MIDI (SysEx) não foi permitido. Permita MIDI nas configurações do site (barra de endereço) e aperte Instalar de novo.",
    notfound: "FM-1 não encontrado. Confira o cabo USB (de dados), feche outros apps que usam MIDI e aperte Instalar de novo.",
    model: "O aparelho conectado não é um FM-1.", lost: "A conexão USB caiu. Reconecte e aperte Instalar de novo.",
    noloader: "O FM-1 em modo de atualização não foi encontrado. Reconecte o cabo e aperte Instalar de novo (ele fica nesse modo até terminar).",
    noreturn: "O FM-1 não voltou. Desligue e ligue de novo.", mismatch: "Gravado, mas o FM-1 informa outra versão: ",
    foreign: "O FM-1 está no modo de atualização de outro firmware. Termine essa atualização com o atualizador dele (M-UPGRADE) e aperte Instalar de novo: ",
    badpkg: "O arquivo de instalação está danificado. Recarregue a página.", leave: "Gravando. Não feche a página.",
    license: "GPL-3.0, sem garantia. Código:", stockTitle: "Voltar ao firmware oficial (V15)",
    stockHelp: "Baixe o FM-1 V15 oficial no site da M-VAVE e escolha o FM-1.fwsc de dentro dele, sem modificar. O arquivo é conferido neste navegador e não é enviado a lugar nenhum.",
    stockBackup: "O firmware oficial não abre músicas, projetos nem samples do SLOOP / winampFM. Faça backup com o editor antes (Projects → Backup); reinstale o winampFM e restaure pelo editor quando quiser.",
    stockEditor: "Abrir o editor para o backup", stockSaved: "Salvei um backup neste computador", stockFile: "O FM-1.fwsc oficial", stockGo: "Instalar o V15 oficial",
    stockValid: "V15 oficial conferido", stockBad: "Não é o V15 oficial: ", stockConfirm: "Instalar o V15 oficial no FM-1? O firmware oficial não abre músicas do winampFM. Você fez backup?",
    stockFinish: "O FM-1 está no modo de atualização do SLOOP / winampFM. Aperte Instalar para terminar essa atualização e depois volte ao V15 oficial.",
    back: "O atualizador da própria M-VAVE, M-UPGRADE (Downloads → PC Software), também funciona. Se ele não achar o FM-1, feche outros apps e abas que usam MIDI.",
  },
};'''
i = js.index('const TEXT = {'); j = js.index('const t = (k)')
body = js[i:j].rstrip()
assert body.endswith('};')
js = js[:i] + body[:-2] + PT + '\n' + js[j:]
js = js.replace('const t = (k) => TEXT.en[k];', 'const t = (k) => (TEXT[document.documentElement.dataset.l] || TEXT.en)[k] ?? TEXT.en[k];\naddEventListener("wfm-lang", () => applyLang());   // (the page switched EN / PT)')
js = js.replace('../editor/', 'editor/')
js = js.replace('Save a backup with the editor first (Projects → Backup); reinstall winampFM and restore it from the editor whenever you want them back.', 'Write down or record what you want to keep first.')
js = js.replace('Faça backup com o editor antes (Projects → Backup); reinstale o winampFM e restaure pelo editor quando quiser.', 'Anote ou grave antes o que quiser guardar.')
js = js.replace('say(`winampFM build ${meta.version}`)', 'sayK("ready", meta.version)')
js = js.replace('    intro: "Connect the FM-1', '    ready: "Ready to install winampFM ",\n    intro: "Connect the FM-1', 1)
js = js.replace('    intro: "Ligue o FM-1', '    ready: "Pronto para instalar o winampFM ",\n    intro: "Ligue o FM-1', 1)
p = Path('site/index.html'); s = p.read_text()
a, b = s.index('<!--INSTALLER-->'), s.index('<!--/INSTALLER-->')
s = s[:a] + '<!--INSTALLER-->\n' + js + '\n' + s[b:]
p.write_text(s)
shutil.rmtree(tmp)
shutil.rmtree('instalador', ignore_errors=True)
print('site installer:', pkg, os.path.getsize(pkg), 'B')
