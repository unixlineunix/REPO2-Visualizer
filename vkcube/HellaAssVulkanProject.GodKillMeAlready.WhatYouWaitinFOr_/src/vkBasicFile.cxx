// Basic file for Vk related stuff

// Idk if this will even be looking like a readable file or not

// Basic includes.
#include "../headers/basicIncludes"
#include "../headers/Utility/externs"
#include "../headers/Utility/reAnsi"
#include "../headers/Core/aCr_s"

namespace IDK {
	bool uf_CX_vkInitStp_1 (){

		try {
			IDK::vCX_uniM.sCX_vkI_aLaunch();
		}

		catch (const rstd::exception& e){
			rstd::ansi::CnsChr::RTUI::prVK_EXCEPTION(e.what());
			//rstd::ansi::CnsChr::RTUI::Csp(((rstd::aString)rstd::ansi::CnsChr::STYLE::BOLD+rstd::ansi::CnsChr::STYLE::ITALIC+rstd::ansi::CnsChr::COLOR::YELLOW+"[VK_EXCEPTION::]"+rstd::ansi::CnsChr::RESET).c_str(),e.what());
			return false;
		}

		return true;
	}

	bool ctCX_vkInitStp_1(IDK::c_cCX_MainApplctN& ap_CX){
		try {
			ap_CX.sCX_vkI_aLaunch();

		} 
		catch (rstd::exception& e_Cx){
			rstd::ansi::CnsChr::RTUI::prVK_EXCEPTION(e_Cx.what());
			return false;
		}

		return true;
	}

	bool AppTerm_CX_Check (){
		if (IDK::GlbAppTerm_CX) {	
			rstd::ansi::CnsChr::RTUI::CXerr("IDK BRUH - "," The app termination request was provided.");
			return false;
		}
		else {
			return true;
		}
	}
}