# QTCP-based-on-repeater-based-quantum-Internet
A simplified simulation about QTCP in classic network.

1. You should install and configure ns-3.27 in Linux.
2. You should install the code from Claypool to implement BBR congestion control. The code is seen in: https://github.com/mark-claypool/bbr
3. Repalce the tcp-tx-buffer.h, tcp-tx-buffer.cc, tcp-socket-base.h, tcp-socket-base.cc in ns-allinone-3.27/ns-3.27/src/internet/model with our files with the same name.
4. Execute p2p.cc to perfrom QTCP in p2p topology. Execute dumbbell.cc to perfrom QTCP in dumbbell topology.
