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

	local CHITCP_HEADER_LEN = 16

	function chitcp.dissector(buffer, pinfo, tree)

		local tcp_dissector = Dissector.get("tcp")
		local offset = 0
		-- Loop until we've processed the entire buffer (a single TCP segment
		-- of the underlying connection can contain several chiTCP packets)
		while offset < buffer:len() do
			local remaining = buffer:len() - offset

			-- If we don't have the complete chiTCP packet, ask Wireshark to
			-- reassemble it with the next TCP segment(s) of the connection
			if remaining < CHITCP_HEADER_LEN then
				pinfo.desegment_offset = offset
				pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
				return
			end

			-- Read the chitcp header
			local payload_len = buffer(offset, 2):uint()

			if remaining < CHITCP_HEADER_LEN + payload_len then
				pinfo.desegment_offset = offset
				pinfo.desegment_len = CHITCP_HEADER_LEN + payload_len - remaining
				return
			end

			local subtree = tree:add(chitcp, buffer(offset, CHITCP_HEADER_LEN + payload_len))
			subtree:add(f.payload_len, buffer(offset, 2))
			subtree:add(f.proto, buffer(offset + 2, 1))
			subtree:add(f.flags, buffer(offset + 3, 1))

			-- Hand off the TCP segment (and only that segment) to the TCP dissector
			if payload_len > 0 then
				local sub_buf = buffer(offset + CHITCP_HEADER_LEN, payload_len):tvb()
				tcp_dissector:call(sub_buf, pinfo, tree)
			end

			offset = offset + CHITCP_HEADER_LEN + payload_len
		end
		pinfo.cols.protocol = chitcp.name
	end

	local tcp_table = DissectorTable.get("tcp.port")
	tcp_table:add(23300, chitcp)
end
