import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: 'BetaRobot',
  description: '多开发板支持的嵌入式机器人控制框架',
  lang: 'zh-CN',

  // GitHub Pages 部署需要 base 与仓库名一致
  base: '/BetaRobot/',

  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: '首页', link: '/' },
      { text: '项目文档', link: '/project_docs/项目架构' },
      { text: '使用文档', link: '/user_docs/使用文档' },
      { text: '官方手册', link: '/official_docs/index' },
      { text: '其他文档', link: '/other_docs/index' },
    ],

    sidebar: {
      '/project_docs/': [
        {
          text: '项目文档',
          items: [
            { text: '项目架构', link: '/project_docs/项目架构' },
            { text: '代码规范', link: '/project_docs/代码规范' },
            { text: 'git使用规范', link: '/project_docs/git使用规范' },
            { text: '代码要求', link: '/project_docs/代码要求' },
            { text: 'Pages开发文档', link: '/project_docs/Pages开发文档' },
            { text: '设计思路和原因', link: '/project_docs/设计思路和原因' },
          ]
        }
      ],
      '/user_docs/': [
        {
          text: '使用文档',
          items: [
            { text: '使用文档', link: '/user_docs/使用文档' },
            { text: 'API说明', link: '/user_docs/API说明' },
          ]
        }
      ],
      // 官方手册已迁移至独立仓库，本站只保留一个跳转页
      '/official_docs/': [
        {
          text: '官方手册',
          items: [
            { text: '资料索引', link: '/official_docs/' },
          ]
        }
      ],
      '/other_docs/': [
        {
          text: '其他文档',
          items: [
            { text: '概述', link: '/other_docs/index' },
          ]
        }
      ],
    },

    // 社交平台图标和链接
    socialLinks: [
      { icon: 'github', link: 'https://github.com/itgz123/BetaRobot' },
    ],

    // 搜索功能
    search: {
      provider: 'local',
    },

    // // 编辑链接
    // editLink: {
    //   pattern: 'https://github.com/itgz123/BetaRobot/edit/main/docs/:path',
    //   text: '在 GitHub 上编辑此页',
    // },

    // 右侧显示所有层级大纲
    outline: 'deep',
  },

  // // 构建输出目录（默认 .vitepress/dist，相对项目根目录 docs/）
  // outDir: '.vitepress/dist',
})
