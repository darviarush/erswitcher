unit Unit1;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, ExtCtrls;

type

  { TForm1 }

  TForm1 = class(TForm)
    ImageList1: TImageList;
    TrayIcon1: TTrayIcon;
    procedure FormActivate(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    function getLayout
  private

  public

  end;

var
  Form1: TForm1;

implementation

{$R *.lfm}

{ TForm1 }

procedure TForm1.FormActivate(Sender: TObject);
begin
  TrayIcon1.Icon.LoadFromFile('ico/');
end;

procedure TForm1.FormCreate(Sender: TObject);
begin

end;

end.

