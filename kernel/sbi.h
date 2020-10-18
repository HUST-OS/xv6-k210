// SBI Call Head file

void sbi_console_putchar(int ch);
int sbi_console_getchar();
void sbi_send_ipi(const unsigned long *hart_mask);