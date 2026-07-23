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
      '/official_docs/': [
        {
          text: '官方手册',
          items: [
            { text: 'BMI088', link: '/official_docs/BMI088/BMI088.pdf' },
            { text: 'C610', link: '/official_docs/C610/RM C610无刷电机调速器使用说明 发布版.pdf' },
            { text: 'C620', link: '/official_docs/C620/RoboMaster__C620_无刷电机调速器使用说明.pdf' },
            { text: 'DJI_A', link: '/official_docs/' },
            { text: 'DJI_C', link: '/official_docs/' },
            { text: 'DM_MC02', link: '/official_docs/' },
            { text: 'DM4310', link: '/official_docs/DM4310/DM-J4310-2EC V1.2减速电机说明书V1.2定稿.pdf' },
            { text: 'GM6020', link: '/official_docs/GM6020/RoboMaster_GM6020直流无刷电机使用说明20231013.pdf' },
            { text: 'IST8310', link: '/official_docs/IST8310/IST8310.pdf' },
            { text: 'M2006', link: '/official_docs/M2006/RM M2006 P36直流无刷减速电机使用说明.pdf' },
            { text: 'M3508', link: '/official_docs/M3508/RoboMaster_M3508_直流无刷电机_使用说明（中英）.pdf' },
            { text: 'MPU6500', link: '/official_docs/MPU6500/MPU6500.pdf' },
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
