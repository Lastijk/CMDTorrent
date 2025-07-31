CMDTorrent is a command line torrent tracker I`m currently working on.
Right now the project has:
- Bencode namespace - A parser of bencoded metadata, useful to decode a torrent file or message. Construct BencodeParser with your data(std::string) and method ParseAny will return your decoded data in std::string. ComputeSHA1 is a helpful method if you`re parsing an actual .torrent file, it returns SHA1 hash of its "info" value.
- TorrentFile.cpp - LoadTorrentFile parses requested .torrent file and puts data in a TorrentFile struct.
- TorrentTracker.cpp - Collects and updates peers, can return them if needed
- TCP_Connect.cpp - Connects to a peer, can send and receive data via respective functions

The work is still on, many things done, I just want to polish the project and release a full working app.
