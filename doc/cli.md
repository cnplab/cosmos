ClickOS Manager tool (`cosmos`)
==============================

It adds the ClickOS specific xenstore interactions plus the VALE port
interface. Main differences are in user interface. While `xl` or `xm`
only accept domains names, our cosmos tool can also use domain IDs. This
is mostly usefull when crashes occur in the backend and a (null) appears
in the domain listing (`xl list`.

Find a trace of the terminal when using `cosmos`.

    # cosmos create etc/xen/test.cfg
	Parsing config file etc/xen/test.cfg
	Created domain with ID 148 in 0.021987 seconds
	VALE ports: 0
    
    # xl list
    Name                                        ID   Mem VCPUs      State   Time(s)
	Domain-0                                     0 10240     3     r-----     727.6
	test                                       148     5     1     -b----       0.0

	# cosmos vale-attach test 00:11:22:33:44:55 Domain-0
	Network attach for id 148 mac 00:11:22:33:44:55 backend 0

    # cosmos vale-attach 148 00:11:22:33:44:55 0
	Network attach for id 148 mac 00:11:22:33:44:55 backend 0

	# cosmos start test etc/click/fromnetfront.click 
	Dom ID 148
	Reading click script: etc/click/fromnetfront.click
	Read 94/94 bytes Click script:
	          in    :: FromNetFront(00:1a:31:a4:60:ff, BURST 256);
			  sink  :: Discard(BURST 256);
			  in -> sink;

			  Started domain with ID 148 in 0.001874 seconds
    
    # cosmos -a start test etc/click/fromnetfront.click
    Auto network-attach enabled
    Dom ID 154
    Reading click script: etc/click/fromnetfront.click
    Read 94/94 bytes Click script:
            in    :: FromNetFront(00:1a:31:a4:60:ff, BURST 256);
    sink  :: Discard(BURST 256);
    in -> sink;

    Started domain with ID 154 in 0.006211 seconds