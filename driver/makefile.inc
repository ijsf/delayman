$(OBJ_PATH)\$(O)\delayman.bmf : delayman.mof
    mofcomp -B:$@ delayman.mof
    wmimofck $@   
    wmimofck -h$(OBJ_PATH)\$(O)\delayman_mof.h $@
