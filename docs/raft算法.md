# raft算法

# 算法准备

## 选举相关的概念

1. 当前角色state（follower，candidate，leader）
2. 任期（current_term）
3. 投给谁了（votefor）
4. 票数（vote_count）
5. 投票结果（granted）支持还是反对
6. 当前领导人（current_leader）

## 日志相关的概念
1. 日志（term，id，type）
2. 日志请求（term，leader_id, prev_log_index, prev_log_term, leader_commit_index, log[]）

## 选举触发相关的概念

1. 心跳超时
2. 选举超时

## 网络相关的概念

1. 节点（peer）
2. rpc投票请求
3. 处理rpc投票请求
4. 处理rpc投票响应
4. 处理rpc心跳（日志）请求
5. 处理rpc心跳（日志）响应
6. 自动重连
7. 处理网络超时

# follower

## follower要做的事

### 处理rpc心跳请求

1. 看心跳是否超时，超时就忽略
2. 如果没有选举超时，并且rpc请求中的任期号比自己小，则忽略这次心跳
3. 如果没有选举超时，并且rpc请求中的任期号不比自己小，则承认该leader的合法地位，设置current_leader为该leader.回到follower状态

4. 没有超时则做日志处理 todo
5. 将等待心跳超时的标识设置为false，无需再等待了。同时将选举超时的标识设置为false，无需再等待了。

### 心跳超时处理

1. 等待心跳超时: 如果没超时就继续保持follower状态, 如果超时了就等待选举超时.
2. 等待选举超时: 如果选举没有超时则继续保持follower状态, 如果超时了就转变为candidate状态.

### 处理投票请求

1. 如果有leader并且没有选举超时则直接回false
2. 如果term < currentTerm 则返回false
3. 如果本地的voteFor为空或者为candidateId（幂等性？）, 并且候选者的日志至少与接受者的日志一样新,则投给其选票
4. 怎么定义日志新
	```
	比较两份日志中最后一条日志条目的索引值和任期号定义谁的日志比较新。
	如果两份日志最后的条目的任期号不同，那么任期号大的日志更加新。
	如果两份日志最后的条目任期号相同，那么日志比较长的那个就更加新。
	```

# candiate

## candiate要做的事

### 发起新的选举

1. 递增current_term
2. 投票给自己
3. 重置election timer
4. 向所有的节点发送rpc投票请求

### 发起投票请求

1. 向所有的peer发送rpc请求

### 处理投票结果

1. 如果获取了多数投票就转换为leader，设置当前leader为自己，并向其他节点发送心跳来维持统治。
2. 如果没有收到多数票，保持candidate状态。

### 处理投票请求

同上

### 心跳超时处理 心跳超时之后又收到心跳的处理（这时，可能正在发起选举或投票）

1. 如果选举超时，则忽略这次心跳
2. 如果没有选举超时，并且rpc请求中的任期号比自己小，则忽略这次心跳
3. 如果没有选举超时，并且rpc请求中的任期号不比自己小，则承认该leader的合法地位，设置current_leader为该leader.回到follower状态

# leader

## leader要做的事

### 广播发送心跳

向所有的节点发送rpc心跳

### 处理心跳响应

1. 如果成功，todo
2. 如果失败，todo

### 处理投票请求

同上

