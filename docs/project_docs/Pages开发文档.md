# 简介

项目使用 VitePress 构建文档站点，通过GitHub Pages发布，所有文档都在 docs/ 目录下。  
目录结构：
```
docs/
├── .vitepress/
│   └── config.mts           // **核心配置文件**（导航栏、侧边栏、主题配置）
├── builder_docs/            // 开发文档
├── node_modules/            // 依赖文件，不提交，每个开发者通过json文件下载依赖
├── official_docs/           // 官方手册
├── project_docs/            // 项目文档
├── user_docs/               // 使用文档
├── other_docs/              // 其他文档
├── index.md                 // 首页
├── package-lock.json        // 依赖配置文件
├── package.json             // 依赖配置文件
└── start-docs.bat           // 本地启动脚本，开发时预览
```

# 开发方法
**需要开发的文件：**  
1. `config.mts`
2. `index.md`
3. 自定义文档目录：`builder_docs/`,`official_docs`,`project_docs/`,`user_docs/`,`other_docs/`
4. 本地启动脚本，开发时预览：`start-docs.bat`

### `config.mts`

##### `nav`配置页面顶部的导航栏链接
它是一个数组。每个项可以是一个普通链接 { text: '显示文本', link: '/路径' }，也可以是一个下拉菜单 { text: '菜单名', items: [ ... ] }。

##### `sidebar`配置页面左侧的侧边栏导航
它可以是一个数组（所有页面共享同一个侧边栏），也可以是一个对象（根据不同的路径前缀显示不同的侧边栏）。

##### `socialLinks`在导航栏的右侧添加社交平台图标和链接
它是一个数组。每个项需要指定 icon 和 link。

##### `search`为站点启用搜索功能
provider: 'local' 表示启用基于浏览器本地索引的全文搜索。

##### `editLink`在每个页面的底部显示一个“编辑此页”的链接，方便访客直接跳转到源文件进行修改
需要指定 pattern，即链接的 URL 模板。

##### `outline`控制页面右侧大纲（即“目录”）显示的标题层级
'deep' 表示显示所有层级的标题（从 h2 到 h6）。你也可以设置为数字（如 2 只显示 h2）或一个范围（如 [2,3] 显示 h2 和 h3）。

