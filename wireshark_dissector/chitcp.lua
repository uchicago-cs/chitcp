-- Quick and dirty ChiTCP dissector for Wireshark
-- Written by: Tristan Rasmussen (modifed from STCP dissector by Borja Sotomayor)
do
	local chitcp = Proto("chitcp", "Chicago Transmission Control Protocol")

	function chitcp.init()
	end

	local f = chitcp.fields
	f.payload_len = ProtoField.uint16("chitcp.payload_len", "Payload Length")
	f.proto = ProtoField.uint8("chitcp.proto", "Protocol")
	f.flags = ProtoField.uint8("chitcp.flags", "Chitcp Flags")

	function chitcp.dissector(buffer, pinfo, tree)

		local offset = 0
		-- Loop until we've processed the entire buffer
		while offset < buffer:len() do
			-- Read the chitcp header
			local payload_len = buffer(offset, 2):uint()
			local proto = buffer(offset + 2, 1):uint()
			local flags = buffer(offset + 3, 1):uint()

			local subtree = tree:add(chitcp, buffer(offset + 16 + payload_len))
			subtree:add(f.payload_len, payload_len)
			subtree:add(f.proto, proto)
			subtree:add(f.flags, flags)

			-- Hand off to TCP dissector
			tcp_dissector = Dissector.get("tcp")
			sub_buf = buffer(offset + 16):tvb()
			tcp_dissector:call(sub_buf, pinfo, tree)

			offset = offset + 16 + payload_len
		end
		pinfo.cols.protocol = chitcp.name
	end

	local tcp_table = DissectorTable.get("tcp.port")
	tcp_table:add(23300, chitcp)
end
