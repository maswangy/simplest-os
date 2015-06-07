#define GPBCON  (*(volatile unsigned long *)0x56000010)
#define GPBDAT  (*(volatile unsigned long *)0x56000014)

void led_blink(void)
{
	GPBCON = 0x00015400;
	while(1)
	{
		GPBDAT = 0x00000000;
		delay();
		GPBDAT = 0xffffffff;
		delay();
	}
}

