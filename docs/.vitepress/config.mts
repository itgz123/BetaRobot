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
      { text: '项目文档', link: '/guide/项目架构设计方案' },
      { text: '开发指南', link: '/guide/APP开发指南' },
      { text: '工具链', link: '/guide/toolchain/' },
      { text: '技术参考', link: '/guide/other/' },
      { text: '官方手册', link: '/manuals/' },
    ],

    sidebar: {
      '/guide/': [
        {
          text: '项目介绍',
          items: [
            { text: '概述', link: '/guide/intro' },
          ]
        },
        {
          text: '项目架构',
          items: [
            { text: '项目架构设计方案', link: '/guide/项目架构设计方案' },
          ]
        },
        {
          text: '开发指南',
          items: [
            { text: 'APP 开发指南', link: '/guide/APP开发指南' },
            { text: 'BSP 开发指南', link: '/guide/BSP开发指南' },
            { text: 'DRV 开发指南', link: '/guide/DRV开发指南' },
          ]
        },
        {
          text: '代码规范',
          items: [
            { text: '代码规范', link: '/guide/代码规范' },
            { text: '代码要求', link: '/guide/代码要求' },
          ]
        },
        {
          text: '配置说明',
          items: [
            { text: '配置文件', link: '/guide/配置文件' },
          ]
        },
        {
          text: 'Git 规范',
          items: [
            { text: 'Git 使用规范', link: '/guide/git使用规范' },
          ]
        },
        {
          text: '检查清单',
          items: [
            { text: 'Check List', link: '/guide/check_list' },
          ]
        },
        {
          text: '工具链',
          items: [
            { text: '概述', link: '/guide/toolchain/' },
            { text: 'SEGGER RTT 使用指南', link: '/guide/toolchain/SEGGER_RTT使用指南' },
            { text: 'VS Code 配置文件详解', link: '/guide/toolchain/VSCode配置文件详解' },
            { text: 'CMake 工具链', link: '/guide/toolchain/cmake工具链' },
            { text: '工具链配置', link: '/guide/toolchain/工具链配置' },
          ]
        },
        {
          text: '技术参考',
          items: [
            { text: '概述', link: '/guide/other/' },
            { text: 'Cortex-M7 内存使用注意事项', link: '/guide/other/Cortex-M7内存使用注意事项' },
            { text: 'CubeMX FreeRTOS 配置指南', link: '/guide/other/CubeMX_FreeRTOS配置指南' },
            { text: 'FreeRTOS 静态内存 API 指南', link: '/guide/other/FreeRTOS静态内存API指南' },
            { text: 'HAL TIM API 参考', link: '/guide/other/HAL_TIM_API参考' },
            { text: 'Makefile 语法教程', link: '/guide/other/Makefile语法教程' },
            { text: 'DWT 使用说明', link: '/guide/other/dwt使用' },
            { text: 'readelf 输出文件说明', link: '/guide/other/readelf输出文件说明' },
            { text: '内存与存储原理', link: '/guide/other/内存与存储原理' },
          ]
        },
      ],

      '/manuals/': [
        {
          text: '官方手册',
          items: [
            { text: '概述', link: '/manuals/' },
            { text: 'BMI088', link: '/manuals/bmi088' },
            { text: 'C610', link: '/manuals/c610' },
            { text: 'C620', link: '/manuals/c620' },
            { text: 'DJI A 型开发板', link: '/manuals/dji_a' },
            { text: 'DJI C 型开发板', link: '/manuals/dji_c' },
            { text: 'DM-MC02 开发板', link: '/manuals/dm_mc02' },
            { text: 'GM6020', link: '/manuals/gm6020' },
            { text: 'IST8310', link: '/manuals/ist8310' },
            { text: 'MPU6500', link: '/manuals/mpu6500' },
            { text: 'DM4310', link: '/manuals/dm4310' },
          ]
        }
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/itgz123/BetaRobot' },
    ],

    // 搜索功能
    search: {
      provider: 'local',
    },

    // 编辑链接
    editLink: {
      pattern: 'https://github.com/itgz123/BetaRobot/edit/main/docs/:path',
      text: '在 GitHub 上编辑此页',
    },

    lastUpdated: true,
    outline: 'deep',
  },

  // 语法高亮：ld（链接器脚本）回退到纯文本
  markdown: {},

  // 忽略外部文件死链接（.doc 目录等）
  ignoreDeadLinks: true,

  // 构建输出目录（默认 .vitepress/dist，相对项目根目录 docs/）
  // outDir: '.vitepress/dist',
})
