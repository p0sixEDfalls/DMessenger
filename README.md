# DMessenger
Proof-of-concept: Decentralized messenger based on Bitcoin v0.1.5

# Introduction
I started this project for my research work at university. It demonstrates how to create decentralized messenger using blockchain technology. Decentralized apps (dapps) allow to avoid some interventions from different sources. For example, conflict of Telegram with Russian government, which can be useless if Telegram be a dapp, because it unreal to block. I believe what dapps will replace traditional apps. 

# Review
Every messenger client identifies as 160-bit ID, generated from Bitcoin public key (Hash160 function by public key). 
For connecting of clients, they should pass the fields of transaction. So, CTransacrtion class was modified, now it includes IP address of client and IDs with which client wants to connect.

![ctrans](https://user-images.githubusercontent.com/30228254/52918016-31818100-3303-11e9-84d0-93c79e1d64f4.jpg)

These transactions will be added to block. Other client read every block and try to find personal ID in every transaction. If client found his ID, he will try to connect to transaction’s IP address, verify, create encryption key and send/receive messages.   

![processblock](https://user-images.githubusercontent.com/30228254/52918029-5f66c580-3303-11e9-8e59-39fa4884a083.jpg)

All messenger actions defined in messenger.cpp/h files.

# PoC
Let client1 wants to connect client2.
Then client1 should:
1.	Start Bitcoin node.
2.	Start messenger.
3.	Mine one block for creating transaction.
4.	Create transaction with own IP and client2 ID.
5.	Mine block again with new transaction.

Then client2 should:
1.	Start Bitcoin node.
2.	Start messenger.
3.	Read every block from client1 node.

Now, when clients are connected, they will try to verify each, create encryption keys and start message.

This is CLI application. There are two types of console view: menu and client dialog.

Menu:
![menu](https://user-images.githubusercontent.com/30228254/52918071-e6b43900-3303-11e9-8055-909e65565d1d.jpg)

Menu have a list of connected clients. Choose he client and press the number of client.

Dialog:
![dialog](https://user-images.githubusercontent.com/30228254/52918080-051a3480-3304-11e9-9ade-6836449bfa55.jpg)

Dialog contain only correspondence.
Type ‘s’ for call a command. Type ‘F1-F12’ for update console.

In menu you can use these commands:

|   Command  |                       Description                      |
|:----------:|:------------------------------------------------------:|
|  /setpaswd | Generate encryption key for dialog from typed password |
| /cnttohash |          Create new connection with  typed ID          |
|  /cnttoip  |           Create new connection with typed IP          |

In dialog you can use only this command:

| Command |        Description        |
|:-------:|:-------------------------:|
|  /menu  | Return to list of dialogs |

# Libraries
Boost v1.35.0 - https://www.boost.org/users/download/

Berkley Database v4.8.30.NC - https://www.oracle.com/technetwork/database/database-technologies/berkeleydb/downloads/index.html

OpenSSL v1.0.2 - https://www.openssl.org/source/

Compiled with VS2015.
