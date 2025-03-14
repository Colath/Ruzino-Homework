# 作业说明
## 目录结构
这个文件夹是我们布置作业所使用的。其中目录结构及含义为：
- assignements
  - data: 我们提供的数据
  - nodes: 每次布置节点相关作业我们会在里面添加相应的模板
  - utils: 我们不会在这里来写东西的。这个文件夹是留给你们使用的。如果需要编写一些多个node都可以共享的内容，那么可以写在这下面
  
## 克隆完成这个项目后如何上手？
1. 复制这个文件夹到当前目录下面(i.e. submission)，并将其改名为**yourname_homework**。比如**zhangsan_homework**。以后你所有的开发工作都**只在这个文件夹下进行**，尽量不要动框架内的东西。
2. 创建好文件夹后，你需要修改一些东西: 
> CMakeLists.txt **第一行的字符串改为你的全名**！比如set(student_name "assignments") 改为set(student_name "zhangsan") 
> 
> 进入nodes文件夹中，将布置作业的节点文件**加上你的名字作为前缀**。比如原来是node_hw0.cpp，我叫张三，你就改为zhangsan_node_hw0.cpp。以后创建节点的时候都加上这个前缀！

3. 在做完上述步骤之后，就可以愉快地写作业力！（喜

## 如何完成作业？
每次作业，我们会在nodes文件夹中创建一些模板节点。你要把这些节点**复制到你自己的文件夹里面的nodes文件夹**，加上**你的名字作为前缀**！

打开前述cpp文件，找到所有的`NODE_DECLARATION_FUNCTION(...)`&`NODE_EXECUTION_FUNCTION(...)`&`NODE_DECLARATION_UI(...)
`&`NODE_DECLARATION_REQUIRED(...)`，这些地方定义了**你的节点名字**，请将其修改为**使用你的名字作为前缀**！比如`assignments_shortest_path` 修改为 `zhangsan_shortest_path`

对于资源的引用，请使用**相对路径**指向你自己的data文件夹下面的资源或者我们的assignments/data文件夹下面的资源。不要把你的资源放在assignments/data下面，**放在你自己的data**里面！

如果你想要编写一些**多节点可以公用的代码**，我们提供了utils文件夹。这下面的所有源文件会直接参与编译，且将生成target名为`${student_name}_${util_lib_target_name}`的静态库，并**自动链接到你的节点上**。

如果你使用了一些**第三方库**，请将他们**放在utils中**，并在utils里面的**CMakeLists.txt中**仿照我们的方式，为前述静态库添加链接！**尽量不要**去使用一些**无法静态链接的库**

## 如何提交作业？
每次提交之前，确保：
1. 你的资源路径都是使用的正确的路径，并以相对路径的方式指向了你的data文件夹
2. 前述所有的命名操作都是对的

然后，你需要**将Assets(项目根目录中)文件夹下的stage.usdc文件复制到你自己的文件夹下**(zhangsan_homework)，然后**整个打包作为提交项**

## 有无示例？
有的兄弟，有的！你会在submissions文件夹下面看到zhangsan_homework，里面有对node_hw0的实现（当然，这**不是你们的作业**，仅仅作为示例！）

仿照这个的修改去构建你们自己的文件夹。一定要完成前面的那些改名项！