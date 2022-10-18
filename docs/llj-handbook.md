1. 在 windows 用 docker 开发

   ```
   docker run -d --name miniob --privileged -v $PWD/miniob-2022:/root/miniob -p 10086:22 oceanbase/miniob
   
   docker exec -it miniob bash
   ```

   vscode 连接：`root@localhost:10086`

   ```
   # vscode ssh 配置如下
   Host miniob-2022-docker
     HostName localhost
     User root
     Port 10086
     ForwardAgent yes
   ```

   

2. 123