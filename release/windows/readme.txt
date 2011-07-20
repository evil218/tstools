码流工具箱

目前有如下工具：
catbin  把二进制文件（*.ts）转换成文本格式，输出到stdout
catip   把IP端口收到的码流转换成文本格式，输出到stdout
tsana   从stdin接收TS包，分析后输出到stdout
tobin   从stdin接收文本格式的TS包，转换成二进制格式，输出到文件（*.ts）

组合使用这些工具，可以灵活的做TS层的各种工作：
show_pid_list.bat
show_psi_tree.bat
ts_record.bat
...

安装：
双击执行install.bat即可把这些工具和它们依赖的dll拷入windows的system32目录。

cygwin1.dll：
这是使为Linux写的软件能在windows下运行的接口，在linux下就不需要这个挺大的dll了。

周骋
