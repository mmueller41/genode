proc use_dynamic_rom { } { return false }

create_boot_directory
import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg] \
                  [depot_user]/src/dynamic_rom \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
				  [depot_user]/src/stdcxx \
                  [depot_user]/src/nic_router \
                  [depot_user]/src/vfs_audit \
                  [depot_user]/src/vfs_lxip \
                  [depot_user]/src/vfs

append config {
<config verbose="yes">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="TOPO"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="timer">
		<resource name="RAM" quantum="16M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="drivers" caps="1000" managing_system="yes">
		<resource name="RAM" quantum="512M"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Uplink"> <child name="nic_router"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="nic_router" caps="200">
		<resource name="RAM" quantum="20M"/>
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="no">

			<policy label_prefix="socket_fs" domain="downlink"/>
			<policy label_prefix="drivers"   domain="uplink"/>

			<domain name="uplink" interface="10.0.2.55/24" gateway="10.0.2.1" verbose_packets="no">

				<nat domain="downlink"
				     tcp-ports="16384"
				     udp-ports="16384"
				     icmp-ids="16384"/>

				<tcp-forward port="80"   domain="downlink" to="10.0.3.55"/>
				<tcp-forward port="8080" domain="downlink" to="10.0.3.55"/>

				<udp-forward port="7"    domain="downlink" to="10.0.3.55"/>
				<udp-forward port="7070" domain="downlink" to="10.0.3.55"/>

			</domain>

			<domain name="downlink" interface="10.0.3.1/24" verbose_packets="no">

				<dhcp-server ip_first="10.0.3.55" ip_last="10.0.3.55">
					<dns-server ip="8.8.8.8"/>
					<dns-server ip="1.1.1.1"/>
				</dhcp-server>

				<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>
				<udp dst="0.0.0.0/0"><permit-any domain="uplink" /></udp>
				<icmp dst="0.0.0.0/0" domain="uplink"/>

			</domain>

		</config>
	</start>
}

append_if [use_dynamic_rom] config {
	<start name="dynamic_rom">
		<resource name="RAM" quantum="16M"/>
		<provides><service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="socket_fs.config"> <inline description="MTU default">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<lxip ip_addr="192.168.0.2" netmask="255.255.255.0" gateway="192.168.0.1" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/> <inline description="MTU 400">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<lxip mtu="400" ip_addr="10.0.2.55" netmask="255.255.255.0" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/>
			</rom>
		</config>
	</start>}

append config {

	<start name="socket_fs" caps="200">
		<binary name="vfs"/>
		<resource name="RAM" quantum="32M"/>
		<provides> <service name="File_system"/> </provides>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service> }

if {[use_dynamic_rom]} { append config {

			<service name="ROM" label="config">
				<child name="dynamic_rom" label="socket_fs.config"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route> }

} else { append config {

			<any-service> <parent/> <any-child/> </any-service>
		</route>
		<config ld_verbose="yes">
			<vfs>
				<dir name="socket">
					<lxip ip_addr="10.0.3.55" netmask="255.255.255.0" gateway="10.0.3.1" nameserver="8.8.8.8"/>
					<!-- <lxip dhcp="yes"/> -->
				</dir>
			</vfs>
			<default-policy root="/socket" writeable="yes" />
		</config> }
}

append config {

	</start> }

append qemu_args " -nographic "
append_qemu_nic_args "host=10.0.2.1,dhcpstart=10.0.2.55,hostfwd=tcp::10080-:80,hostfwd=tcp::18080-:8080,hostfwd=udp::10007-:7,hostfwd=udp::17070-:7070"


# vi: set ft=tcl 
