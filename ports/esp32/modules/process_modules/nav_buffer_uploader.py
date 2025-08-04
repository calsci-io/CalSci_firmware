class Tbf:
    def __init__(self, disp_out, chrs, m_b):
        self.disp_out=disp_out
        self.chrs=chrs
        self.m_b=m_b
        # self.clear=bytearray(10) #edit with framebuffer module
        self.disp_out.graphics(bytearray(128), page=7, column=0, width=128, pages=1)
        #from data_modules.object_handler import display
        #display.graphics(bytearray(128), page=7, column=0, width=128, pages=1)
        
    def refresh(self, state="default"):
        # x=bytearray(10) #edit with framebuffer module
        self.disp_out.graphics(self.clear, page=8, column=1, width=len(x), pages=1)
        # buf=self.m_b.buffer()
        self.disp_out.set_page_address(7)
        for i in self.m_b.buffer():
            chtr_byte_data=self.chrs.invert_letter(i)
            for j in chtr_byte_data:
                self.disp_out.write_data(k)
            
        # ref_rows=self.m_b.ref_ar()
        for i in range(ref_rows[0], ref_rows[1]):
            self.disp_out.set_page_address(i)
            self.disp_out.set_column_address(0)
            buf[i]+=" "*(self.m_b.cols-len(buf[i]))
            for j in buf[i]:
                if i == self.m_b.cursor():
                    chtr_byte_data=self.chrs.invert_letter(j)
                    cursor_line=0b11111111
                    for k in chtr_byte_data:
                        self.disp_out.write_data(k)
                    self.disp_out.write_data(cursor_line)
                else:
                    chtr_byte_data=self.chrs.Chr2bytes(j)
                    cursor_line=0b00000000
                    for k in chtr_byte_data:
                        self.disp_out.write_data(k)
                    self.disp_out.write_data(cursor_line)
        self.disp_out.set_page_address(7)
        self.disp_out.set_column_address(0)
        j_counter=0
        for j in state:
            chtr=j
            chtr_byte_data=self.chrs.invert_letter(chtr)
            cursor_line=0b11111111
            for k in chtr_byte_data:
                self.disp_out.write_data(k)
            self.disp_out.write_data(cursor_line)
            j_counter+=1