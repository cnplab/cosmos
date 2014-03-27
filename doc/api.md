ClickOS Manager API (COSMOS)
============================

The methods are the same whatever language you generate your binding with.

C
=

Read the source.

Python
======

To quickly test, please compile and go to `<clickos-tools-dir>/build/python`, 
and run the following script:

```python
    import cosmos

    xen_config = "./etc/xen/test.cfg";
    click_script = open("./etc/click/fromnetfront.click").read();
    
    flags = 1;
    # Initializes a context for cosmos with verbose enabled.
    # <flags> consists of a bitmask which
    # 0x01 = enable verbose
    # 0x10 = enable autoattach in clickos_start
    cosmos.ctx(flags)

    # Create the domain
    domid = cosmos.create(xen_config);

    # Create the domain with <name> and <kernel>
    #  plus default options
    domid = cosmos.create_default("clickos", 
		"<path to click>/clickos/build/bin/click-os-x86_64")
    
    # Network attach for vm with <domid>, <mac-addr>, <backend-domid>, <bridge>
    cosmos.network_attach(domid, "00:11:22:33:44:55", 0, "")

    # Start the domain
    cosmos.start(domid, "script", click_script);
    
    # Destroy the domain
    if cosmos.destroy(domid) < 0:
        print "Domain invalid"

    # Frees the context
    cosmos.free()
```

Javascript
==========

To quickly test, please compile everything and also do `$ make javascript-binding V8=1`, 
then just run the following script in the cosmos root folder:

```javascript
	var cosmos = require('../build/Release/cosmos')
	  , fs = require('fs');

	flags = 0;
	
	cosmos.ctx(flags);
	
	/* Create a VM */
	domid = cosmos.create_default("test", 
		                "<path to click>/clickos/build/click_os_x86_64");
	
	/* Attaches a virtual interface */
	cosmos.network_attach(domid, "11:22:33:44:55:66", 0, "");
	
	fs.readFile("./etc/click/elements/idle.click", function(err, click_script){

    /* Instantiates a middlebox */
    cosmos.start(domid, "script", click_script+"");
	});

	function destroy() {
	    cosmos.destroy(domid);    
	    cosmos.free();    
	}
	
	setTimeout(destroy, 2000);
```
