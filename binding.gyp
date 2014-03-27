{
  "targets": [
    {
      "target_name": "cosmos",
      "sources": [ 
        "build/javascript/cosmos_wrap.cxx",        
      ],
      "include_dirs": [
        "-I/root/cosmos/include"
      ],
      "libraries": [
        "-lcosmos -L/root/cosmos/build/lib"
      ]
    }
  ]
}
