in    :: FromDevice();
//sink   :: ToDevice();
sink  :: Discard(BURST 1024);
in -> sink;
