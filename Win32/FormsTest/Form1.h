#pragma once


extern "C" {
#include <directfb.h>

#include <direct/thread.h>

#include <voodoo/play.h>
}

using namespace System::Runtime::InteropServices;

namespace FormsTest {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Summary for Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::Button^  player_list_update;
	protected: 

	private: System::Windows::Forms::ListBox^  player_list;
	protected: 
				VoodooPlayer *player;
				 IDirectFBSurface *surface;
	private: System::Windows::Forms::Button^  red;
	private: System::Windows::Forms::Button^  green;
	private: System::Windows::Forms::Button^  yellow;
	private: System::Windows::Forms::Button^  blue;
	private: System::Windows::Forms::TextBox^  addressTextLine;
	protected: 

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->player_list_update = (gcnew System::Windows::Forms::Button());
			this->player_list = (gcnew System::Windows::Forms::ListBox());
			this->red = (gcnew System::Windows::Forms::Button());
			this->green = (gcnew System::Windows::Forms::Button());
			this->yellow = (gcnew System::Windows::Forms::Button());
			this->blue = (gcnew System::Windows::Forms::Button());
			this->addressTextLine = (gcnew System::Windows::Forms::TextBox());
			this->SuspendLayout();
			// 
			// player_list_update
			// 
			this->player_list_update->Location = System::Drawing::Point(12, 151);
			this->player_list_update->Name = L"player_list_update";
			this->player_list_update->Size = System::Drawing::Size(286, 36);
			this->player_list_update->TabIndex = 0;
			this->player_list_update->Text = L"Update list";
			this->player_list_update->UseVisualStyleBackColor = true;
			this->player_list_update->Click += gcnew System::EventHandler(this, &Form1::button1_Click);
			// 
			// player_list
			// 
			this->player_list->FormattingEnabled = true;
			this->player_list->Location = System::Drawing::Point(12, 37);
			this->player_list->Name = L"player_list";
			this->player_list->Size = System::Drawing::Size(286, 108);
			this->player_list->TabIndex = 1;
			this->player_list->SelectedValueChanged += gcnew System::EventHandler(this, &Form1::player_list_SelectedValueChanged);
			// 
			// red
			// 
			this->red->ForeColor = System::Drawing::Color::Red;
			this->red->Location = System::Drawing::Point(12, 200);
			this->red->Name = L"red";
			this->red->Size = System::Drawing::Size(67, 43);
			this->red->TabIndex = 2;
			this->red->Text = L"red";
			this->red->UseVisualStyleBackColor = true;
			this->red->Visible = false;
			this->red->Click += gcnew System::EventHandler(this, &Form1::red_Click);
			// 
			// green
			// 
			this->green->ForeColor = System::Drawing::Color::LimeGreen;
			this->green->Location = System::Drawing::Point(85, 200);
			this->green->Name = L"green";
			this->green->Size = System::Drawing::Size(67, 43);
			this->green->TabIndex = 3;
			this->green->Text = L"green";
			this->green->UseVisualStyleBackColor = true;
			this->green->Visible = false;
			this->green->Click += gcnew System::EventHandler(this, &Form1::green_Click);
			// 
			// yellow
			// 
			this->yellow->ForeColor = System::Drawing::Color::Olive;
			this->yellow->Location = System::Drawing::Point(158, 200);
			this->yellow->Name = L"yellow";
			this->yellow->Size = System::Drawing::Size(67, 43);
			this->yellow->TabIndex = 4;
			this->yellow->Text = L"yellow";
			this->yellow->UseVisualStyleBackColor = true;
			this->yellow->Visible = false;
			this->yellow->Click += gcnew System::EventHandler(this, &Form1::yellow_Click);
			// 
			// blue
			// 
			this->blue->ForeColor = System::Drawing::Color::Blue;
			this->blue->Location = System::Drawing::Point(231, 200);
			this->blue->Name = L"blue";
			this->blue->Size = System::Drawing::Size(67, 43);
			this->blue->TabIndex = 5;
			this->blue->Text = L"blue";
			this->blue->UseVisualStyleBackColor = true;
			this->blue->Visible = false;
			this->blue->Click += gcnew System::EventHandler(this, &Form1::blue_Click);
			// 
			// addressTextLine
			// 
			this->addressTextLine->Location = System::Drawing::Point(12, 12);
			this->addressTextLine->Name = L"addressTextLine";
			this->addressTextLine->Size = System::Drawing::Size(286, 20);
			this->addressTextLine->TabIndex = 6;
			this->addressTextLine->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Form1::addressTextLine_KeyDown);
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(313, 257);
			this->Controls->Add(this->addressTextLine);
			this->Controls->Add(this->blue);
			this->Controls->Add(this->yellow);
			this->Controls->Add(this->green);
			this->Controls->Add(this->red);
			this->Controls->Add(this->player_list);
			this->Controls->Add(this->player_list_update);
			this->Name = L"Form1";
			this->Text = L"DirectFB";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
	private:
		DirectEnumerationResult
		player_callback( void                    *ctx,
				         const VoodooPlayInfo    *info,
						 const VoodooPlayVersion *version,
						 const char              *address,
						 unsigned int             ms_since_last_seen )
		{
		//	D_INFO( "Voodoo/Play: <%4ums> [ %-30s ]   %s%s\n",
		//			 ms_since_last_seen, info->name, address, (info->flags & VPIF_LEVEL2) ? " *" : "" );

			player_list->Items->Add( gcnew String(address) );

			return DENUM_OK;
		}

	public:
		[UnmanagedFunctionPointer(CallingConvention::Cdecl)]	delegate DirectEnumerationResult
		PC( void                    *ctx,
				         const VoodooPlayInfo    *info,
						 const VoodooPlayVersion *version,
						 const char              *address,
						 unsigned int             ms_since_last_seen );

	private:
		System::Void button1_Click(System::Object^  sender, System::EventArgs^  e) {
				PC ^pc = gcnew PC( this, &Form1::player_callback );

				player_list->BeginUpdate();
				player_list->Items->Clear();
				
				voodoo_player_enumerate( player, (VoodooPlayerCallback) Marshal::GetFunctionPointerForDelegate(pc).ToPointer(), NULL );
				
				player_list->EndUpdate();


				voodoo_player_broadcast( player );
			 }

	private:
		System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e) {
				DirectResult  ret;
				VoodooPlayer *p;

				ret = voodoo_player_create( NULL, &p );
				if (ret) {
					D_ERROR( "Voodoo/Play: Could not create the player (%s)!\n", DirectFBErrorString((DFBResult)ret) );
					return;
				}

				player = p;

				voodoo_player_broadcast( player );


				surface = NULL;
			 }

	private:
		System::Void player_list_SelectedValueChanged(System::Object^  sender, System::EventArgs^  e) {
			if (!surface) {
				 DFBResult    ret;
				 IDirectFB   *dfb;

				 DirectFBInit( NULL, NULL );

				 IntPtr str = Marshal::StringToHGlobalAnsi( player_list->GetItemText( player_list->SelectedItem ) );
				 DirectFBSetOption( "remote", (char*) str.ToPointer() );
				 Marshal::FreeHGlobal( str );

				 ret = DirectFBCreate( &dfb );
				 if (ret) {
					 addressTextLine->Text = gcnew String(DirectFBErrorString(ret));
					 return;
				 }

				 IDirectFBSurface *s = NULL;
				 DFBSurfaceDescription desc;

				 desc.flags = DSDESC_CAPS;
				 desc.caps  = DSCAPS_PRIMARY;

				 dfb->CreateSurface( dfb, &desc, &s );
				 surface = s;

				 if (surface) {
					 red->Show();
					 green->Show();
					 yellow->Show();
					 blue->Show();

					 player_list->Enabled = FALSE;
				 }
			}
		}

	private:
		System::Void red_Click(System::Object^  sender, System::EventArgs^  e) {
			if (surface) {
				surface->Clear( surface, 0xff, 0x00, 0x00, 0x80 );
				surface->Flip( surface, NULL, DSFLIP_NONE );
			}
		}

	private:
		System::Void green_Click(System::Object^  sender, System::EventArgs^  e) {
			if (surface) {
				surface->Clear( surface, 0x00, 0xff, 0x00, 0x80 );
				surface->Flip( surface, NULL, DSFLIP_NONE );
			}
		}

	private:
		System::Void yellow_Click(System::Object^  sender, System::EventArgs^  e) {
			if (surface) {
				surface->Clear( surface, 0xff, 0xff, 0x00, 0x80 );
				surface->Flip( surface, NULL, DSFLIP_NONE );
			}
		}

	private:
		System::Void blue_Click(System::Object^  sender, System::EventArgs^  e) {
			if (surface) {
				surface->Clear( surface, 0x00, 0x00, 0xff, 0x80 );
				surface->Flip( surface, NULL, DSFLIP_NONE );
			}
		}
private: System::Void addressTextLine_KeyDown(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e) {
			 if (e->KeyCode == Keys::Enter) {
				 player_list->Items->Add( addressTextLine->Text );
			 }
		 }
};
}

