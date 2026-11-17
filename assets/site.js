(() => {
  'use strict';

  // These statements are transcribed from the supplied exhibition panel photograph.
  const translations = {
    ja: {
      title: 'Dynamic Heartbeat — 東京大学制作展 2026',
      description: '手のひらに伝わる鼓動に、生命を感じますか。人の心臓の拍動を人工物で再現し、「生きている」と感じる私たちの認識を問い直す作品。',
      skip: '本文へ移動',
      language: '言語を選択',
      exhibition: '東京大学制作展 2026',
      projectLinks: '作品の関連リンク',
      invitation: '手のひらに、生命の気配。',
      explore: '作品について',
      heroAlt: '暗い展示空間のトレイに置かれ、チューブにつながれた人工の心臓',
      detailAlt: 'Dynamic Heartbeatのキービジュアル。暗い青紫の背景に、発光する心臓と作品名。',
      question: '人は、何を手がかりに\n「生きている」と\n感じるのでしょうか。',
      statementOne: 'その一つに、鼓動があるかもしれません。胸に手を当ててみてください。伝わってくる拍動は、私たちに生きている実感を与えてくれます。けれど、普段は直接触れることのない自分の心臓を、手で確かめてみたいと思ったことはありませんか。',
      statementTwo: 'Physical AIが発展すれば、人工物の動作や表情は、生き物と見分けがつかなくなるかもしれません。そのとき、身体の内側から伝わる鼓動は、私たちが生命を感じる手がかりになるのでしょうか。',
      touch: '触れてみる。\n生命を、感じてみる。',
      statementThree: '本作品では、人の心臓の拍動を人工物でできるだけリアルに再現しました。目の前にある心臓を自分のものとして感じながら、触れてみてください。頭では人工物だとわかっていても、手のひらに伝わる鼓動に生命を感じることがあるかもしれません。その感覚を通して、「生きている」と感じる私たちの認識を問い直します。',
      filmTitle: '動きはじめる、鼓動。',
      videoLabel: 'Dynamic Heartbeatの拍動を撮影した映像',
      downloadVideo: '映像をダウンロードする',
      systemDiagramSource: 'figs/system_diagram.svg',
      systemScrollLabel: 'システム構成図。横にスクロールできます。',
      systemDiagramAlt: 'ArduinoがMOSFET経由で空気ポンプと弁1・弁2を制御するシステム構成図。駆動電源は12V ACアダプタで、水冷ポンプは電源に直結。制御信号・電源・空気・液体の経路を線種で区別しています。',
      creators: 'Creators',
      tsubaki: '椿 道智',
      sugimoto: '杉本 隆平',
      tsubakiAffiliation: '東京大学大学院 学際情報学府',
      tsubakiLab: '岡田・小島・河原塚研究室（情報システム工学研究室）',
      sugimotoAffiliation: '東京大学大学院 学際情報学府',
      sugimotoLab: '葛岡・鳴海・谷川研究室（システム工学研究室）',
      miki: '三木 章寛',
      okada: '岡田 慧',
      mikiRole: '特任助教',
      okadaRole: '教授',
      backToTop: 'ページの先頭へ',
      copyCitation: 'コピー',
      citationCopied: 'コピーしました',
      citationSelected: '選択しました。手動でコピーしてください'
    },
    en: {
      title: 'Dynamic Heartbeat — The University of Tokyo Exhibition 2026',
      description: 'How alive does it feel to you? An artificial heart recreates the human heartbeat, inviting us to reconsider how we perceive something as alive.',
      skip: 'Skip to content',
      language: 'Choose language',
      exhibition: 'The University of Tokyo Exhibition 2026',
      projectLinks: 'Explore the project',
      invitation: 'A heartbeat. A sense of life.',
      explore: 'Explore the work',
      heroAlt: 'An artificial heart connected to tubes, resting on a tray in a dark exhibition space',
      detailAlt: 'Dynamic Heartbeat key visual: a glowing heart and the work’s title against a dark blue and purple background.',
      question: 'What makes us feel\nthat we are alive?',
      statementOne: 'A heartbeat may be one such cue. Place a hand on your chest. The beat you feel gives you a sense of being alive. Yet have you ever wished you could touch your own heart directly and feel it with your hand?',
      statementTwo: 'As Physical AI advances, the movements and expressions of machines may become indistinguishable from those of living beings. In such a future, could a heartbeat from within the body help us sense the presence of life?',
      touch: 'Reach out.\nFeel a sense of life.',
      statementThree: 'This work recreates the human heartbeat as realistically as possible in an artificial heart. Reach out and touch the heart before you, imagining it as your own. Even when you know it is artificial, the beat you feel in the palm of your hand may make it seem alive. Through this sensation, the work invites us to reconsider how we perceive something as “alive.”',
      filmTitle: 'A heartbeat, in motion.',
      videoLabel: 'Film showing the beating of Dynamic Heartbeat',
      downloadVideo: 'Download the film',
      systemDiagramSource: 'figs/system_diagram.en.svg',
      systemScrollLabel: 'System diagram. Scroll horizontally to explore.',
      systemDiagramAlt: 'System diagram: the Arduino controls the air pump and both valves through MOSFETs. A 12V AC adapter supplies actuator power; the liquid pump is powered directly. Line styles distinguish control signals, power, air, and liquid.',
      creators: 'Creators',
      tsubaki: 'Michitoshi Tsubaki',
      sugimoto: 'Ryuhei Sugimoto',
      tsubakiAffiliation: 'Graduate School of Interdisciplinary Information Studies, The University of Tokyo',
      tsubakiLab: 'JSK Robotics Lab.',
      sugimotoAffiliation: 'Graduate School of Interdisciplinary Information Studies, The University of Tokyo',
      sugimotoLab: 'Cyber Interface Lab.',
      miki: 'Akihiro Miki (JSK)',
      okada: 'Kei Okada (JSK)',
      mikiRole: 'Project Assistant Professor',
      okadaRole: 'Professor',
      backToTop: 'Back to top',
      copyCitation: 'Copy',
      citationCopied: 'Copied',
      citationSelected: 'Selected. Please copy manually'
    }
  };

  const preferenceKey = 'dynamic-heartbeat-language';
  const buttons = document.querySelectorAll('[data-language]');

  function setLanguage(language, updateUrl = false) {
    const copy = translations[language];
    if (!copy) return;
    document.documentElement.lang = language;
    document.title = copy.title;
    document.querySelector('meta[name="description"]').content = copy.description;

    document.querySelectorAll('[data-i18n]').forEach(element => {
      const value = copy[element.dataset.i18n];
      if (value === undefined) return;
      // Use text nodes so translated content never becomes executable HTML.
      const lines = value.split('\n');
      element.replaceChildren();
      lines.forEach((line, index) => {
        if (index) element.append(document.createElement('br'));
        element.append(document.createTextNode(line));
      });
    });

    document.querySelectorAll('[data-i18n-alt]').forEach(element => {
      element.alt = copy[element.dataset.i18nAlt];
    });
    document.querySelectorAll('[data-i18n-aria]').forEach(element => {
      element.setAttribute('aria-label', copy[element.dataset.i18nAria]);
    });
    document.querySelectorAll('[data-i18n-src]').forEach(element => {
      element.setAttribute('src', copy[element.dataset.i18nSrc]);
    });
    document.querySelectorAll('[data-i18n-href]').forEach(element => {
      element.setAttribute('href', copy[element.dataset.i18nHref]);
    });
    buttons.forEach(button => {
      button.setAttribute('aria-pressed', String(button.dataset.language === language));
    });

    if (updateUrl) {
      try { localStorage.setItem(preferenceKey, language); } catch { /* Storage may be unavailable. */ }
      try {
        const url = new URL(window.location.href);
        url.searchParams.set('lang', language);
        history.replaceState(null, '', url);
      } catch { /* Language switching also works when opened as a local file. */ }
    }
  }

  let savedLanguage;
  try { savedLanguage = localStorage.getItem(preferenceKey); } catch { /* Use browser language. */ }
  const requestedLanguage = new URLSearchParams(window.location.search).get('lang');
  const browserLanguage = navigator.language.toLowerCase().startsWith('ja') ? 'ja' : 'en';
  setLanguage([requestedLanguage, savedLanguage, browserLanguage].find(language => Object.hasOwn(translations, language)));
  buttons.forEach(button => button.addEventListener('click', () => setLanguage(button.dataset.language, true)));

  const copyButton = document.querySelector('.citation-copy');
  const citationCode = document.querySelector('.language-bibtex');
  if (copyButton && citationCode) {
    copyButton.hidden = false;
    const label = copyButton.querySelector('[data-i18n]');
    let resetTimer;
    copyButton.addEventListener('click', async () => {
      clearTimeout(resetTimer);
      copyButton.disabled = true;
      const text = citationCode.textContent.trim() + '\n';
      let copied = false;
      try {
        await navigator.clipboard.writeText(text);
        copied = true;
      } catch {
        // Support local files and browsers that deny clipboard access.
        const field = document.createElement('textarea');
        field.value = text;
        field.style.cssText = 'position:fixed;left:-9999px;top:0';
        document.body.append(field);
        try {
          field.select();
          copied = document.execCommand('copy');
        } catch { /* Select the visible code if copying is unavailable. */ }
        field.remove();
      }
      copyButton.disabled = false;
      copyButton.focus({ preventScroll: true });
      if (!copied) {
        const range = document.createRange();
        range.selectNodeContents(citationCode);
        const selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
      }
      label.dataset.i18n = copied ? 'citationCopied' : 'citationSelected';
      label.textContent = translations[document.documentElement.lang][label.dataset.i18n];
      resetTimer = setTimeout(() => {
        label.dataset.i18n = 'copyCitation';
        label.textContent = translations[document.documentElement.lang].copyCitation;
      }, 2500);
    });
  }
})();
