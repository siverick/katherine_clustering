
/**
 * @main.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "stdafx.h"
#include "main_program.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Main GUI thread is in main_program.h class
    main_program w;
    w.show();

    return a.exec();
}
