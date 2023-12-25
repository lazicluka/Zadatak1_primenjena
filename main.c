
#include <stdio.h>
#include <stdlib.h>
#include <p30fxxxx.h>
#include <string.h>
#include <time.h>  
#include "driverGLCD.h" 
#include "adc.h"
//#include <outcompare.h> //za pwm ali ga ne koristimo na kraju
//#include "tajmer.h"   // isto


#define DRIVE_A PORTCbits.RC13 // definisemo za folije za touch
#define DRIVE_B PORTCbits.RC14

_FOSC(CSW_FSCM_OFF &XT_PLL4); // instruction takt je isti kao i kristal
//_FOSC(CSW_ON_FSCM_OFF & HS3_PLL4);
_FWDT(WDT_OFF);
_FGS(CODE_PROT_OFF);


// const unsigned int ADC_THRESHOLD = 900;
const unsigned int AD_Xmin = 220;
const unsigned int AD_Xmax = 3642;
const unsigned int AD_Ymin = 520;
const unsigned int AD_Ymax = 3450;

unsigned int X, Y, x_vrednost, y_vrednost;

unsigned char tempRX;
unsigned int i, j , k, hit, win, r1, r2, duzina, pirflag;
int buf;
char otkriveno[20];
int zivoti = 6; // zivota kao 6 delova tela
int pomoc = 3;  
unsigned int sirovi0, sirovi1, sirovi2, sirovi3;
unsigned int broj1, broj2, temp0, temp1, PIR, FR, ton, duz;
//unsigned int brojac_ms, stoperica, ms, sekund; // ZA TAJMER


char grad[10][20] = {                          //gradovi
    "Novi Sad", "Kragujevac", "Kraljevo",
    "Beograd", "Valjevo", "Subotica", "Jagodina",
    "Loznica", "Sombor", "Zrenjanin"};

void ConfigureTSPins(void)
{
    // ADPCFGbits.PCFG10=1;5
    // ADPCFGbits.PCFG7=digital;

    // TRISBbits.TRISB10=0;
    TRISCbits.TRISC13 = 0;
    TRISCbits.TRISC14 = 0;

    // LATCbits.LATC14=0;
    // LATCbits.LATC13=0;
}

void initUART1(void)
{
    U1BRG = 0x0081; // ovim odredjujemo baudrate,pri menjaju frekvecije menjamo takodje

    U1MODEbits.ALTIO = 0; // biramo koje pinove koristimo za komunikaciju osnovne ili alternativne

    IEC0bits.U1RXIE = 1; // omogucavamo rx1 interupt

    U1STA &= 0xfffc;

    U1MODEbits.UARTEN = 1; // ukljucujemo ovaj modul

    U1STAbits.UTXEN = 1; // ukljucujemo predaju
}

void __attribute__((__interrupt__,no_auto_psv)) _U1RXInterrupt(void) //no_auto_psv?
{
    IFS0bits.U1RXIF = 0;
    tempRX = U1RXREG;

    if ((tempRX >= 'a' && tempRX <= 'z') || (tempRX >= 'A' && tempRX <= 'Z'))  
    {                 // proverava da li je uneto malo ili veliko slovo
        buf = tempRX; // smesta u buf promenljivu ako jeste
    };
}

void __attribute__((__interrupt__,no_auto_psv)) _ADCInterrupt(void)
{
    sirovi0 = ADCBUF0; 
    sirovi1 = ADCBUF1; 
    sirovi2 = ADCBUF2; 
    sirovi3 = ADCBUF3; 

    FR = sirovi0; // smeštamo vrednosti sa ad konverzije radi lakseg praćenja
    PIR = sirovi1;
    temp0 = sirovi2;
    temp1 = sirovi3;

    IFS0bits.ADIF = 0;
}

/*********************************************************************
 * Ime funkcije      : WriteUART1                                     *
 * Opis              : Funkcija upisuje podatak u registar 			 *
 * 					  za slanje podataka U1TXREG,   				 *
 * Parametri         : unsigned int data-podatak koji treba poslati   *
 * Povratna vrednost : Nema                                           *
 *********************************************************************/

void WriteUART1(unsigned int data)
{
    while (!U1STAbits.TRMT)
        ;

    if (U1MODEbits.PDSEL == 3)
        U1TXREG = data;
    else
        U1TXREG = data & 0xFF;
}

void RS232_putst(register const char *str) // funkcija za ispis na terminal,kao za domaci iz serijske
{
    while ((*str) != 0)
    {
        WriteUART1(*str);
        str++;
    }
}
/*
void WriteUART1dec2string(unsigned int data)
{
    unsigned char temp;

    temp = data / 1000;
    WriteUART1(temp + '0');
    data = data - temp * 1000;
    temp = data / 100;
    WriteUART1(temp + '0');
    data = data - temp * 100;
    temp = data / 10;
    WriteUART1(temp + '0');
    data = data - temp * 10;
    WriteUART1(data + '0');
}
*/
void Delay(unsigned int N) // koriscen za touch posle adc folija
{
    unsigned int i;
    for (i = 0; i < N; i++)
        ;
}

void Touch_Panel(void)
{
    // vode horizontalni tranzistori
    DRIVE_A = 1;
    DRIVE_B = 0;

    LATCbits.LATC13 = 1;
    LATCbits.LATC14 = 0;

    Delay(500); // cekamo jedno vreme da se odradi AD konverzija

    // ocitavamo x
    x_vrednost = temp0; // temp0 je vrednost koji nam daje AD konvertor na BOTTOM pinu

    // vode vertikalni tranzistori
    LATCbits.LATC13 = 0;
    LATCbits.LATC14 = 1;
    DRIVE_A = 0;
    DRIVE_B = 1;

    Delay(500); // cekamo jedno vreme da se odradi AD konverzija

    // ocitavamo y
    y_vrednost = temp1; // temp1 je vrednost koji nam daje AD konvertor na LEFT pinu

    // Ako ?elimo da nam X i Y koordinate budu kao rezolucija ekrana 128x64 treba skalirati vrednosti x_vrednost i y_vrednost tako da budu u opsegu od 0-128 odnosno 0-64
    // skaliranje x-koordinate

    X = (x_vrednost - 161) * 0.03629;

    // X= ((x_vrednost-AD_Xmin)/(AD_Xmax-AD_Xmin))*128;
    // vrednosti AD_Xmin i AD_Xmax su minimalne i maksimalne vrednosti koje daje AD konvertor za touch panel.

    // Skaliranje Y-koordinate
    Y = ((y_vrednost - 500) * 0.020725);

    //	Y= ((y_vrednost-AD_Ymin)/(AD_Ymax-AD_Ymin))*64;
}
/*
void Write_GLCD(unsigned int data)
{
    unsigned char temp;

    temp = data / 1000;
    Glcd_PutChar(temp + '0');
    data = data - temp * 1000;
    temp = data / 100;
    Glcd_PutChar(temp + '0');
    data = data - temp * 100;
    temp = data / 10;
    Glcd_PutChar(temp + '0');
    data = data - temp * 10;
    Glcd_PutChar(data + '0');
}
*/
// funkcije za aktivaciju bazera
void buzzer()                   //najkrace,kad se pogresi slovo
{
    for (k = 0; k < 10; k++)
    {
        for (ton = 0; ton < 20; ton++)
        {
            LATAbits.LATA11 = ~LATAbits.LATA11;  // buzzer ja na pinu A11 povezan preko jumpera pa ga konstanto invertujemo 
            for (duz = 0; duz < 100; duz++)
                ;
        }
        for (broj1 = 0; broj1 < 100; broj1++)
            ;
    }
}

void buzzer_lose()           //duze,kad se izgubi
{
    for (k = 0; k < 50; k++)
    {
        for (ton = 0; ton < 30; ton++)
        {
            LATAbits.LATA11 = ~LATAbits.LATA11;
            for (duz = 0; duz < 150; duz++)
                ;
        }
        for (broj1 = 0; broj1 < 100; broj1++)
            ;
    }
}

void buzzer_win()           //najduze,kad se pobedi
{
    for (k = 0; k < 50; k++)
    {
        for (ton = 0; ton < 60; ton++)
        {
            LATAbits.LATA11 = ~LATAbits.LATA11;
            for (duz = 0; duz < 200; duz++)
                ;
        }
        for (broj1 = 0; broj1 < 100; broj1++)
            ;
    }
}




int main(int argc, char **argv)
{
    TRISAbits.TRISA11 = 0; // konfigurisemo kao izlaz,za buzzer
    TRISDbits.TRISD9 = 1;  // za PIR, taster koji mora da se pritisne za njegov rad,kao ulaz definisan

    // konfiguracije i inicijalizacije :
    ConfigureLCDPins();
    ConfigureTSPins();
    GLCD_LcdInit();
    GLCD_ClrScr();
    initUART1();
    ADCinit();
    ConfigureADCPins();

    ADCON1bits.ADON = 1; //pocetak AD konverzije 
    pirflag = 0; // flag za aktivaciju pir-a,stavljen na 0

    
    r1 = FR % 10; // pri deljenju vrednosti sa fotoR(fr) sa % 10 daje ostatak 
    for (duzina = 0; grad[r1][duzina] != '\0'; duzina++)
        ; // racunanje duzina stringa,to jeste koliko slova ima odabrani grad
    for (i = 0; i < duzina; i++)
    {
        if (grad[r1][i] == ' ')
            otkriveno[i] = ' ';
        else
            otkriveno[i] = '_'; // da otkriveno bude niz __ duzine odabranog grada
    };
    //pomoc = 3;

    while (1)
    {
        
        GLCD_ClrScr();
        RS232_putst(otkriveno);
        WriteUART1(13); // novi red
        GoToXY(50, 2);
        GLCD_Printf(otkriveno);
        GoToXY(5, 4);
        GLCD_Printf("Pomoc PIR-a: ");
        for (i = 0; i < pomoc; i++)
        {
            GLCD_Printf("* ");                        // ovaj deo se ponavlja i sluzi za postavljanje interfejsa na GLCD
        }
        GoToXY(5, 6);
        GLCD_Printf("Zivoti: ");
        for (i = 0; i < zivoti; i++)
        {
            GLCD_Printf("<3");
        }
        GLCD_Rectangle(5, 5, 41, 17);
        GoToXY(10, 1);
        GLCD_Printf("RESET");

        while (buf == 0)
        { // cekanje dok korisnik ne unese nesto u serijsku komunikaciju,to jeste dok ne unesemo slovo neko
            
            Touch_Panel();
            
         
            if (x_vrednost < 500 && y_vrednost > 1800 && FR < 100) //fotoR jako osvetljen,pa tek onda moze da se resetuje
            { // reset dugme na touchu,kad se klikne sve se ponovo radi
                for (broj1 = 0; broj1 < 300; broj1++)
                    for (broj2 = 0; broj2 < 100; broj2++)
                        ;      // delay pre provere
                Touch_Panel(); 
                if (x_vrednost < 500) // dodatna provera X ose, desavase da ne prepozna promenu na njoj
                {
                    r1 = FR % 10; // isto kao pre,moglo je i da se koristi nepovezan pin
                    for (i = 0; i < 20; i++)
                    {
                        otkriveno[i] = '\0';
                    }
                    for (duzina = 0; grad[r1][duzina] != '\0'; duzina++)
                        ; // racunanje duzina stringa,to jeste koliko slova ima odabrani grad
                    for (i = 0; i < duzina; i++)
                    {
                        if (grad[r1][i] == ' ')
                            otkriveno[i] = ' ';
                        else
                            otkriveno[i] = '_'; // da otkriveno bude niz __ duzine odabranog grada
                    };
                    pomoc = 3; //vracaju se na pocetne vrednosti
                    zivoti = 6;

                    GLCD_ClrScr();
                    RS232_putst(otkriveno);
                    WriteUART1(13); // novi red
                    GoToXY(50, 2);
                    GLCD_Printf(otkriveno);
                    GoToXY(5, 4);
                    GLCD_Printf("Pomoc PIR-a: ");
                    for (i = 0; i < pomoc; i++)
                    {
                        GLCD_Printf("* ");
                    }
                    GoToXY(5, 6);
                    GLCD_Printf("Zivoti: ");
                    for (i = 0; i < zivoti; i++)
                    {
                        GLCD_Printf("<3");
                    }
                    GLCD_Rectangle(5, 5, 41, 17);
                    GoToXY(10, 1);
                    GLCD_Printf("RESET");
                }
            } // kraj za RESET dugme

            if (PIR > 2500 && pomoc > 0 && pirflag == 0 && PORTDbits.RD9 == 1 ) //uslovi koji moraju biti ispunjeni da bi se PIR triggerovao
            {
                pomoc--;     // gubimo 1 od 3
                pirflag = 1; //dizemo na 1 kad aktiviramo pomoc
                j = 0;
                for (i = 0; i < duzina; i++)
                { // prolazi kroz nasu rec,to jeste grad slovo po slovo,ako nadje prazno mesto inkrementuje j,j=broju neotkriveh slova
                    if (otkriveno[i] == '_')
                        j++;
                }
                r2 = rand() % j;       //promenjiva r2 uvek ce imati manju vrednost od j,kad podelima random broj sa % j
                j = 0;
                for (i = 0; i < duzina; i++)
                {// prolazi kroz nasu rec opet,to jeste grad slovo po slovo,ako nadje prazno mesto onda poredi j(koje krece opet od 0) 
                 // i promennjivu r2 ako su one jednake otkriva slovo na toj lokaciji,povecava j dok se ne poklope da bi se otkrilo
                    if (otkriveno[i] == '_') 
                    {
                        if (j == r2)
                            otkriveno[i] = grad[r1][i];
                        j++;
                    }
                }
                GLCD_ClrScr();
                RS232_putst(otkriveno);
                WriteUART1(13); // novi red
                GoToXY(50, 2);
                GLCD_Printf(otkriveno);
                GoToXY(5, 4);
                GLCD_Printf("Pomoc PIR-a: ");
                for (i = 0; i < pomoc; i++)
                {
                    GLCD_Printf("* ");
                }
                GoToXY(5, 6);
                GLCD_Printf("Zivoti: ");
                for (i = 0; i < zivoti; i++)
                {
                    GLCD_Printf("<3");
                }
                GLCD_Rectangle(5, 5, 41, 17);
                GoToXY(10, 1);
                GLCD_Printf("RESET");
            }
            if (PIR < 2500) // ako je mala vrednost sa pira,to jeste ako pir ne detektuje , flag na 0
                pirflag = 0;

        }; // od cekanja unosa sa serijske

        hit = 1; // flag za oduzimanje zivota, po defaultu treba da oduzme
        win = 1;
        for (i = 0; i < duzina; i++)
        {
            if (buf == grad[r1][i])
            {
                otkriveno[i] = grad[r1][i]; // otkrivanje slova
                hit = 0;                    // ako je pronadjeno slovo onda spusti flag
            }
        }
        for (i = 0; i < duzina; i++)
        {// ako nema praznog mesta igrac je pobedio
            if (otkriveno[i] == '_')
            {
                win = 0; // ako nadje prazno mesto spusti flag za pobedu
            }
        }

        if (hit == 1)
        {
            zivoti--;       //pogresno slovo uneto
            buzzer();
        }

        if (win == 1)
        { // ako je pogodjeno sve
            GLCD_ClrScr();
            RS232_putst("POBEDILI STE");
            WriteUART1(13); // novi red
            GoToXY(40, 2);
            GLCD_Printf("POBEDILI STE");
            buzzer_win();
            WriteUART1(13); // novi red
            WriteUART1(13); // novi red
            for (i = 0; i < 3000; i++)
                for (j = 0; j < 750; j++)
                    ;

            GLCD_ClrScr();

            for (i = 0; i < 20; i++) //praznimo otkriveno,da bi novu rec izbrojali i upisali _
            {
                otkriveno[i] = '\0';
            }

            zivoti = 6; //vracemo inicijalne vrednosti, nova runda
            pomoc = 3;
            r1 = FR % 10; // kao i pre u zavisnosti od vrednosti sa fotoR,grad se nasumicno bira
            for (duzina = 0; grad[r1][duzina] != '\0'; duzina++)
                ; // racunanje duzina stringa,to jeste koliko slova ima odabrani grad
            for (i = 0; i < duzina; i++)
            {
                if (grad[r1][i] == ' ')
                    otkriveno[i] = ' ';
                else
                    otkriveno[i] = '_'; // da otkriveno bude niz __ duzine odabranog grada
            };
        }

        if (zivoti == 0)
        { // ako je GAMEOVER onda da se uradi ispis i reset
            GLCD_ClrScr();
            RS232_putst("GAME OVER");
            WriteUART1(13); // novi red
            GoToXY(40, 2);
            GLCD_Printf("GAME OVER");

            RS232_putst("Resenje: ");

            GoToXY(40, 4);
            GLCD_Printf("Resenje: ");
            RS232_putst(grad[r1]);
            WriteUART1(13); // novi red
            WriteUART1(13); // novi red
            GoToXY(40, 5);
            GLCD_Printf(grad[r1]);
            buzzer_lose();
            for (i = 0; i < 3000; i++)
            {
                for (j = 0; j < 750; j++)
                    ;
            }
            GLCD_ClrScr();
            for (i = 0; i < 20; i++)
            {
                otkriveno[i] = '\0';
            }
            zivoti = 6; //vracenje zivota i pomoci opet
            pomoc = 3;
            r1 = FR % 10; // kao i pre u zavisnosti od vrednosti sa fotoR,grad se nasumicno bira
            for (duzina = 0; grad[r1][duzina] != '\0'; duzina++)
                ; // racunanje duzina stringa,to jeste koliko slova ima odabrani grad
            for (i = 0; i < duzina; i++)
            {
                if (grad[r1][i] == ' ')
                    otkriveno[i] = ' ';
                else
                    otkriveno[i] = '_'; // da otkriveno bude niz __ duzine odabranog grada
            };
        }
        buf = 0; // praznjenje buf promenljive
        
        

    } // od whilea

    return (EXIT_SUCCESS);
}
