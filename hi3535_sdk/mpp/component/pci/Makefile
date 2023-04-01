# $(ROOT)/mpp/code/component/pci/Makefile

ifeq ($(PARAM_FILE), ) 
    PARAM_FILE:=../../Makefile.param
    include $(PARAM_FILE)
endif

DCI_HEADER := dci.h
VIU_HEADER := hi_comm_video.h
MKP_HEADER := mkp_ioctl.h mkp_sys.h mkp_vb.h mkp_vpss.h
EXT_HEADER := vgs_ext.h mm_ext.h mod_ext.h pciv_fmwext.h proc_ext.h sys_ext.h vb_ext.h vpss_ext.h region_ext.h

INC_DIR=$(SDK_PATH)/mpp/component/pci/include

.PHONY: default clean

default:
ifeq ($(MPP_BUILD), y)
	@-pushd $(SDK_PATH)/mpp/component/pci/include > /dev/null; rm -f $(VIU_HEADER) rm -f $(MKP_HEADER); rm -f $(EXT_HEADER); popd > /dev/null
	@-pushd $(SDK_PATH)/mpp/code/mkp_inc > /dev/null;                      cp $(MKP_HEADER) $(INC_DIR); popd > /dev/null
	@-pushd $(SDK_PATH)/mpp/code/mkp/include > /dev/null;                  cp $(EXT_HEADER) $(INC_DIR); popd > /dev/null
	@-pushd $(SDK_PATH)/mpp/code/include > /dev/null;                      cp $(VIU_HEADER) $(INC_DIR); popd > /dev/null
	@-pushd $(SDK_PATH)/mpp/code/arch/$(ARCH_DIR)/include > /dev/null;     cp $(DCI_HEADER) $(INC_DIR); popd > /dev/null
endif
	@for x in `find ./ -maxdepth 2 -mindepth 2 -name "Makefile" `; \
	   do cd `dirname $$x`; if [ $$? ]; then make || exit 1; cd ../; fi; done

#@for x in `find -name Makefile`; do if [ -d $$x ]; then cd $$x; make; cd ../; fi; done

clean:
	@for x in `find ./ -maxdepth 2 -mindepth 2 -name "Makefile" `; \
	   do cd `dirname $$x`; if [ $$? ]; then make clean || exit 1; cd ../; fi; done

#@for x in `ls `; do if [ -d $$x ]; then cd $$x; make clean; cd ../; fi; done;

ifeq ($(MPP_BUILD), y)
	@-pushd $(SDK_PATH)/mpp/component/pci/include > /dev/null; rm -f $(VIU_HEADER) rm -f $(MKP_HEADER); rm -f $(EXT_HEADER); popd > /dev/null
endif

