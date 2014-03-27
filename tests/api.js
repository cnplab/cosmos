var cosmos = require('../build/Release/cosmos')
  , fs = require('fs');

flags = 0;

cosmos.ctx(flags);

// Create a VM
console.time('create');
domid = cosmos.create_default("test", 
                "/root/click/clickos/build/click_os_x86_64");
console.timeEnd('create');

// Attaches a virtual interface
console.time('network-attach');
cosmos.network_attach(domid, "11:22:33:44:55:66", 0, "");
console.timeEnd('network-attach');

fs.readFile("./etc/click/elements/idle.click", function(err, click_script){

    // Instantiates a middlebox
    console.time('start')
    cosmos.start(domid, "script", click_script+"");
    console.timeEnd('start')        
});

function destroy() {
    cosmos.destroy(domid);    
    cosmos.free();    
}

setTimeout(destroy, 2000);
