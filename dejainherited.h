#pragma once
/*
 this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchWinnerSet",
 std::bind(&DejaVu::HandleWinnerSet, this, std::placeholders::_1)); this->gameWrapper->HookEvent("Function
 TAGame.GameEvent_TA.OnCanVoteForfeitChanged", std::bind(&DejaVu::HandleForfeitChanged, this, std::placeholders::_1));
 this->gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated",
 std::bind(&DejaVu::HandleGameTimeUpdate, this, std::placeholders::_1));
 https://github.com/adamk33n3r/Deja-Vu/blob/57387c89924ec14acb7d7e06b6e8b3a06fa36307/DejaVu.cpp#L246



 https://github.com/adamk33n3r/Deja-Vu/blob/57387c89924ec14acb7d7e06b6e8b3a06fa36307/DejaVu.cpp#L754
need to check if your team was the ones that forfeited? because it reports that "YOU WON" even when you forfeit while a
replay is ocurring... so like, a winner isn't set, but it checks for one.


*/
