// docmd.config.js
module.exports = {
  // Core Metadata
  siteTitle: "tBMP",
  siteUrl: "https://NellowTCS.github.io/tBMP/docs",

  // Branding
  logo: {
    alt: "tBMP",
    href: "./",
  },
  favicon: "",

  // Source & Output
  srcDir: "docs",
  outputDir: "site",

  // Theme & Layout
  theme: {
    name: "sky",
    defaultMode: "system",
    enableModeToggle: true,
    positionMode: "top",
    codeHighlight: true,
    customCss: [],
  },

  // Features
  search: true,
  minify: true,
  autoTitleFromH1: true,
  copyCode: true,
  pageNavigation: true,

  // Navigation (Sidebar)
  navigation: [
    { title: "Home", path: "/", icon: "home" },
    {
      title: "Getting Started",
      icon: "rocket",
      collapsible: false,
      children: [
        {
          title: "Quick Start",
          path: "/getting-started/quickstart",
          icon: "play",
        },
        {
          title: "Core Concepts",
          path: "/getting-started/concepts",
          icon: "book",
        },
      ],
    },
    {
      title: "Guide",
      icon: "book-open",
      collapsible: false,
      children: [
        {
          title: "Encoding",
          path: "/guide/encoding",
          icon: "zap",
        },
        {
          title: "Pixel Formats",
          path: "/guide/pixel-formats",
          icon: "image",
        },
        {
          title: "Metadata",
          path: "/guide/metadata",
          icon: "file-text",
        },
      ],
    },
    {
      title: "Reference",
      icon: "file-code",
      collapsible: false,
      children: [{ title: "API", path: "/api/", icon: "box" }],
    },
    {
      title: "GitHub",
      path: "https://github.com/NellowTCS/tBMP",
      icon: "github",
      external: true,
    },
  ],

  // Plugins
  plugins: {
    seo: {
      defaultDescription:
        "A tiny bitmap format for compact, efficient image storage and decoding.",
      openGraph: {
        defaultImage: "",
      },
      twitter: {
        cardType: "summary_large_image",
      },
    },
    sitemap: {
      defaultChangefreq: "weekly",
      defaultPriority: 0.8,
    },
  },

  // Footer
  footer:
    "Built with [docmd](https://docmd.io). [View on GitHub](https://github.com/NellowTCS/tBMP).",

  // Edit Link
  editLink: {
    enabled: true,
    baseUrl: "https://github.com/NellowTCS/tBMP/edit/main/Docs/docs",
    text: "Edit this page",
  },
};
