c :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800,-);
arpq :: ARPQuerier(10.0.20.2, 00:15:17:15:5d:74);
arpr :: ARPResponder(10.0.20.2 00:15:17:15:5d:74);

in     :: FromDevice;
out    :: ToDevice;

inc    :: Counter();
outc   :: Counter();

in -> inc -> c;

c[0] -> arpr;
arpr -> out;
arpq -> out;

c[1] -> [1]arpq;
Idle -> [0]arpq;

c[3] -> Print("What?") -> Discard;
c[2] -> CheckIPHeader(14) -> Print("ICMP") -> ICMPPingResponder() -> EtherMirror() -> outc -> out;
