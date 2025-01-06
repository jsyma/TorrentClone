# Torrent Clone

This project is based on the development of a Peer-to-Peer (P2P) network using socket programming in C. The primary objective is to establish a decentralized network where peers
can exchange content among themselves through the support of the index server. The communication between the index server and a peer is based on UDP while the content download is 
based on TCP. 

## Functionalities 
- **User Registration**: Users can register to participate in the P2P network for uploading and downloading content.
- **Content Registration**: A peer can register content with the index server, providing detials such as content name and server address.
- **Content Search**: Users can search for specific content through the index server, which responds with the address of a content server if available. 
- **Content Download**: Peers can download content directly from other peers by establishing a connection with the content server.
- **Content Listing**: Users can request the index server for a list of all registered content in the network.
- **Content De-Registration**: Peers can remove their content from the index server when it is no longer available for sharing.
- **Quit**: Prior to exiting the network, a peer deregisters all its content to keep the index server's registry accurate. 
