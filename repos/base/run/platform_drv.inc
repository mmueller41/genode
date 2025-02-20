proc have_platform_drv {} {
	return [expr [have_board pc]]
}


##
# Return name of the USB driver binary
#
proc usb_host_drv_binary { } {
	if {[have_board rpi]}             { return legacy_rpi_usb_host_drv       }
	if {[have_board imx6q_sabrelite]} { return legacy_imx6q_sabrelite_usb_host_drv }
	if {[have_board pc]}              { return pc_usb_host_drv }
	return no_usb_drv_available
}


##
# Return name of the audio driver binary
#
proc audio_drv_binary { } {
	if {[have_board linux]} { return linux_audio_drv }
	if {[have_board pc]}    { return pci_audio_drv }
	return no_audio_drv_available
}

##
# Return attributes of the audio driver's <start> node
#
proc audio_drv_start_attr { } {
	if {[have_board linux]} { return {ld="no"} }
	return ""
}

proc acpi_drv_name { } {
	global use_acpica_as_acpi_drv
	if {[info exists use_acpica_as_acpi_drv] && $use_acpica_as_acpi_drv} {
		return acpica }

	return acpi_drv
}

proc platform_drv_build_components {} {
	set drv_build_components ""
	if {[have_board pc]} {
		lappend drv_build_components drivers/platform/legacy/x86
		lappend drv_build_components server/report_rom
		if {[acpi_drv_name] eq "acpi_drv"} {
			lappend drv_build_components drivers/acpi
		}
		if {[acpi_drv_name] eq "acpica"} {
			lappend drv_build_components app/acpica
		}
	}
	return $drv_build_components
}


proc append_platform_drv_build_components {} {
	global build_components
	append build_components { } [platform_drv_build_components]
}

proc platform_drv_binary {} {
	if {[have_board pc]} { return legacy_pc_platform_drv }
	return no_platform_drv_available
}

proc platform_drv_boot_modules {} {
	set drv_boot_modules ""
	lappend_if [have_platform_drv] drv_boot_modules [platform_drv_binary]

	if {[have_board pc]} {
		lappend drv_boot_modules report_rom
		lappend drv_boot_modules [acpi_drv_name]
	}

	return $drv_boot_modules
}

proc append_platform_drv_boot_modules {} {
	global boot_modules
	append boot_modules { } [platform_drv_boot_modules]
}


proc platform_drv_policy {} {

	if {![have_board pc]} {
		return {}
	}

	set drv_policy ""

	if {[acpi_drv_name] eq "acpica"} {
		append drv_policy {
			<policy label="acpi_drv -> ">    <pci class="ALL"/> </policy>}
	}

	append drv_policy {
			<policy label_prefix="ps2_drv">  <device name="PS2"/> </policy>
			<policy label_prefix="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label_prefix="fb_drv">   <pci class="VGA"/> </policy>
			<policy label_prefix="wifi_drv" msix="false"> <pci class="WIFI"/> </policy>
			<policy label_prefix="usb_drv">  <pci class="USB"/> </policy>
			<policy label_prefix="ahci_drv"> <pci class="AHCI"/> </policy>
			<policy label_prefix="nvme_drv"> <pci class="NVME"/> </policy>
			<policy label_prefix="audio_drv"> <pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label_prefix="intel_fb_drv">
				<pci class="VGA"/>
				<pci bus="0" device="0" function="0"/>
				<pci class="ISABRIDGE"/>
			</policy>}

	return $drv_policy
}


proc platform_drv_priority {} { return "" }
proc platform_drv_add_routing {} {

	if {[acpi_drv_name] eq "acpica"} {
		return {
			<service name="ROM" label="acpi_ready"> <child name="acpi_report_rom"/> </service>}
	}

	return ""
}


proc platform_drv_config_config {} {
	if {[acpi_drv_name] eq "acpica"} {
		return {
		<config acpi_ready="yes">}
	}

	return {<config>}
}


proc platform_drv_config {} {
	set drv_config ""

	if {[have_board pc]} {

		append drv_config {
	<start name="acpi_drv" } [platform_drv_priority] { caps="350" >
		<binary name="} [acpi_drv_name] {"/>}

		if {[acpi_drv_name] eq "acpica"} {
			append drv_config {
		<resource name="RAM" quantum="5M"/>
		<config acpi_ready="yes" act_as_acpi_drv="yes" report="yes"/>}
		} else {
			append drv_config {
		<resource name="RAM" quantum="4M"/>}
		}

		append drv_config {
		<route>
			<service name="IO_MEM"> <parent/> </service>
			<service name="LOG">    <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="RM">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
			<service name="TOPO">	<parent/> </service>}

		append_if [expr {[acpi_drv_name] eq "acpica"}] drv_config {
			<service name="IO_PORT">  <parent/> </service>
			<service name="IRQ">      <parent/> </service>
			<service name="Timer">    <child name="timer"/> </service>
			<service name="Platform"> <child name="platform_drv"/> </service>
			<service name="Acpi">     <child name="platform_drv"/> </service>}

		append drv_config {
			<service name="Report"> <child name="acpi_report_rom"/> </service>
			<service name="ROM" label="platform_info"> <parent/> </service>
		</route>
	</start>}

		append drv_config "
	<start name=\"acpi_report_rom\" [platform_drv_priority]>"

		append drv_config {
		<binary name="report_rom"/>
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="ROM" />
			<service name="Report" />
		</provides>
		<config>
			<policy label="intel_fb_drv -> intel_opregion" report="acpi_drv -> intel_opregion"/>
			<policy label="smbios_decoder -> smbios_table" report="acpi_drv -> smbios_table"/>
			<policy label="platform_drv -> acpi"           report="acpi_drv -> acpi"/>}

		append_if [expr {[acpi_drv_name] eq "acpica"}] drv_config {
			<policy label="platform_drv -> acpi_ready" report="acpi_drv -> acpi_ready"/>}

		append drv_config {
		</config>
		<route>
			<service name="LOG"> <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
		</route>
	</start>}

	}

	if {[have_platform_drv]} {

		append drv_config {
	<start name="platform_drv" } [platform_drv_priority] { caps="800" managing_system="yes">
		<binary name="} [platform_drv_binary] {"/>
		<resource name="RAM" quantum="4M"/>
		<provides>
			<service name="Platform"/>}

		append_if [have_board pc] drv_config {
			<service name="Acpi"/>}

		append_if [have_spec arm] drv_config {
			<service name="Regulator"/>}

		append drv_config {
		</provides>
		<route>}

		append drv_config "[platform_drv_add_routing]"

		append_if [have_board pc] drv_config {
			<service name="ROM" label="acpi"> <child name="acpi_report_rom"/> </service>}

		append_if [expr [have_board pc]] drv_config {
			<service name="Report"> <child name="acpi_report_rom"/> </service>}

		append_if [expr [have_board rpi] || [have_board pc]] drv_config {
			<service name="Timer"> <any-child/> </service>}

		append drv_config {
			<any-service> <parent/> </any-service>
		</route>}

		append drv_config [platform_drv_config_config]
		append drv_config [platform_drv_policy]

		append drv_config {
		</config>
	</start>}

	}

	return $drv_config
}


proc append_platform_drv_config {} {
	global config
	append config [platform_drv_config]
	return $config
}
