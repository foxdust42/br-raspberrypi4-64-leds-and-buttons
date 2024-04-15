################################################################################
#
# gpio-progs
#
################################################################################

GPIO_PROGS_VERSION = 1.0
GPIO_PROGS_SITE = ./package/gpio-progs/src
GPIO_PROGS_SITE_METHOD = local

define GPIO_PROGS_BUILD_CMDS
	$(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D)
endef

define GPIO_PROGS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/gpio-progs $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))

