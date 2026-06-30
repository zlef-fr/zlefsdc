/* ZlefSDC landing i18n — EN default + FR.
 * 2 locales => no picker (per the DA): resolve silently via the sitewide
 * zl-lang cookie, then navigator.language, then English. */
(function () {
  const DICT = {
    en: {
      nav_features: 'Features', nav_settings: 'Settings', nav_layouts: 'Layouts', nav_install: 'Install',
      hero_pill: 'MPRIS · real-time · zero tokens',
      hero_h1_html: 'Your current track, <span class="grad">in your panel</span>',
      hero_lede: "ZlefSDC puts the album cover, title and artist of whatever you're playing right in your panel — with play/pause, previous and next a single click away. It reads the player live over MPRIS, so there are no logins, no API keys, and it works with Spotify or any compliant player.",
      hero_get: 'Install it', hero_source: 'View source',
      feat_title: 'Built to disappear into your panel',
      f1_t: 'Live over MPRIS', f1_d: 'Reacts to D-Bus property changes the instant a track or play state changes — no polling, no tokens, no network round-trips.',
      f2_t: 'Any player', f2_d: 'Spotify by default, or set the target to “auto” and it grabs whatever is running — VLC, mpv, browsers, anything MPRIS.',
      f3_t: 'Album art, done right', f3_d: 'Cover-fit, rounded corners, async fetch with caching — for both local and remote artwork. No layout jumps.',
      f4_t: 'Permissive by design', f4_d: 'Toggle every element, reorder them, size the cover, format the text, scroll long titles, theme the colours — live, no restart.',
      f5_t: 'Quick actions', f5_d: 'Bind clicks and scroll to play/pause, next, previous, raise the player, or run any command. Your panel, your shortcuts.',
      f6_t: 'DE-agnostic core', f6_d: 'The whole UI is one reusable GTK widget. xfce4-panel ships first; a new host (GNOME, Waybar, Plasma) is a tiny adapter.',
      set_title: 'Control the entire render',
      set_sub: 'Same widget, same settings panel. Arrange elements into rows and columns by nesting with brackets — each of these is a real render, and yours updates instantly.',
      ex_stacked_t: 'Rows & columns', ex_stacked_d: 'Nest with brackets: cover beside a column of title, controls and progress.',
      ex_full_t: 'Everything on', ex_full_d: 'Cover, metadata, transport and a progress bar.',
      ex_compact_t: 'Minimal', ex_compact_d: 'A small round cover, the title, one button.',
      ex_textonly_t: 'No cover', ex_textonly_d: 'App icon, inline title · artist, full transport.',
      ex_tinted_t: 'Themed', ex_tinted_d: 'Custom text colour and layout, with progress.',
      lay_title: 'Layout recipes',
      lay_sub: 'Pick a <strong>Preset</strong> in Properties → Layout for one-click arrangements — or paste an order string into the <strong>Order</strong> field (or <code>[layout] order=</code> in the config). “,” keeps elements on one line; “[ … ]” starts a new row or column.',
      lay_mediabar: 'Full-height cover · media bar',
      lay_mediabar_note: 'Preset “Full-height cover”. Cover spans the height; title & artist on top; progress with controls at its right end.',
      lay_inline: 'Single row (default)', lay_stacked: 'Cover + stacked column', lay_tworows: 'Two rows',
      lay_controlsleft: 'Controls first', lay_coverless: 'No cover, stacked', lay_buttonscolumn: 'Buttons in a column',
      inst_title: 'Install',
      inst_sub: 'From the signed zlef package repositories — or build it from source.',
      inst_deb: 'Debian / Ubuntu — apt.zlef.fr', inst_arch: 'Arch — arch.zlef.fr',
      inst_build: 'From source', inst_add: 'Add it to the panel',
      inst_add_d: 'In Xfce: <strong>Panel → Add New Items… → ZlefSDC</strong>. Right-click → Properties to open the settings.',
      inst_alt: 'No supported panel?',
      inst_alt_d: 'Run the standalone window host — same widget, anywhere GTK runs:',
      inst_gh: 'Full docs on GitHub',
      ft_made: 'Part of zlef.fr',
    },
    fr: {
      nav_features: 'Fonctions', nav_settings: 'Réglages', nav_layouts: 'Dispositions', nav_install: 'Installer',
      hero_pill: 'MPRIS · temps réel · zéro jeton',
      hero_h1_html: 'Votre morceau en cours, <span class="grad">dans votre panneau</span>',
      hero_lede: "ZlefSDC affiche la pochette, le titre et l'artiste de ce que vous écoutez directement dans votre panneau — avec lecture/pause, précédent et suivant à un clic. Il lit le lecteur en direct via MPRIS : aucune connexion, aucune clé d'API, et ça marche avec Spotify ou n'importe quel lecteur compatible.",
      hero_get: 'Installer', hero_source: 'Voir le code',
      feat_title: 'Conçu pour se fondre dans votre panneau',
      f1_t: 'En direct via MPRIS', f1_d: "Réagit aux changements de propriétés D-Bus dès qu'un morceau ou l'état de lecture change — sans interrogation, sans jeton, sans requête réseau.",
      f2_t: "N'importe quel lecteur", f2_d: 'Spotify par défaut, ou réglez la cible sur « auto » et il prend ce qui tourne — VLC, mpv, navigateurs, tout ce qui parle MPRIS.',
      f3_t: 'La pochette, comme il faut', f3_d: 'Recadrage, coins arrondis, chargement asynchrone avec cache — pour les pochettes locales comme distantes. Aucun saut de mise en page.',
      f4_t: 'Permissif par conception', f4_d: 'Activez chaque élément, réorganisez-les, dimensionnez la pochette, formatez le texte, faites défiler les titres longs, changez les couleurs — en direct, sans redémarrage.',
      f5_t: 'Actions rapides', f5_d: "Associez clics et molette à lecture/pause, suivant, précédent, afficher le lecteur, ou lancer une commande. Votre panneau, vos raccourcis.",
      f6_t: 'Cœur indépendant du bureau', f6_d: "Toute l'interface tient dans un widget GTK réutilisable. xfce4-panel d'abord ; un nouvel hôte (GNOME, Waybar, Plasma) est un petit adaptateur.",
      set_title: 'Contrôlez tout le rendu',
      set_sub: 'Le même widget, le même panneau de réglages. Disposez les éléments en rangées et colonnes grâce aux crochets — chaque vignette est un vrai rendu, et le vôtre se met à jour instantanément.',
      ex_stacked_t: 'Rangées & colonnes', ex_stacked_d: 'Imbriquez avec des crochets : la pochette à côté d’une colonne titre, contrôles et progression.',
      ex_full_t: 'Tout activé', ex_full_d: 'Pochette, métadonnées, contrôles et barre de progression.',
      ex_compact_t: 'Minimal', ex_compact_d: 'Une petite pochette ronde, le titre, un bouton.',
      ex_textonly_t: 'Sans pochette', ex_textonly_d: 'Icône, titre · artiste en ligne, contrôles complets.',
      ex_tinted_t: 'Personnalisé', ex_tinted_d: 'Couleur de texte et disposition sur mesure, avec progression.',
      lay_title: 'Recettes de disposition',
      lay_sub: 'Choisissez un <strong>Préréglage</strong> dans Propriétés → Disposition pour des agencements en un clic — ou collez une chaîne d’ordre dans le champ <strong>Ordre</strong> (ou <code>[layout] order=</code> dans la config). « , » garde les éléments sur une ligne ; « [ … ] » ouvre une nouvelle rangée ou colonne.',
      lay_mediabar: 'Pochette pleine hauteur · barre média',
      lay_mediabar_note: 'Préréglage « Pochette pleine hauteur ». La pochette occupe toute la hauteur ; titre et artiste en haut ; progression avec les contrôles à son extrémité droite.',
      lay_inline: 'Une seule ligne (défaut)', lay_stacked: 'Pochette + colonne empilée', lay_tworows: 'Deux rangées',
      lay_controlsleft: 'Contrôles en premier', lay_coverless: 'Sans pochette, empilé', lay_buttonscolumn: 'Boutons en colonne',
      inst_title: 'Installation',
      inst_sub: 'Depuis les dépôts de paquets signés zlef — ou compilez depuis les sources.',
      inst_deb: 'Debian / Ubuntu — apt.zlef.fr', inst_arch: 'Arch — arch.zlef.fr',
      inst_build: 'Depuis les sources', inst_add: 'Ajouter au panneau',
      inst_add_d: 'Dans Xfce : <strong>Panneau → Ajouter de nouveaux éléments… → ZlefSDC</strong>. Clic droit → Propriétés pour les réglages.',
      inst_alt: 'Pas de panneau pris en charge ?',
      inst_alt_d: "Lancez l'hôte fenêtre autonome — le même widget, partout où GTK tourne :",
      inst_gh: 'Doc complète sur GitHub',
      ft_made: 'Fait partie de zlef.fr',
    },
  };

  function readCookie(name) {
    return document.cookie.split('; ').find(c => c.startsWith(name + '='))?.split('=')[1];
  }
  const lang = (readCookie('zl-lang') || navigator.language || 'en').slice(0, 2).toLowerCase();
  const t = DICT[lang] || DICT.en;

  document.documentElement.lang = DICT[lang] ? lang : 'en';
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const v = t[el.dataset.i18n];
    if (v == null) return;
    if (el.dataset.i18n.endsWith('_html') || /<\w/.test(v)) el.innerHTML = v;
    else el.textContent = v;
  });
})();
