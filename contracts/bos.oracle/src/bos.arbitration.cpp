/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/time.hpp>
#include <string>
#include "bos.oracle/bos.oracle.hpp"
// namespace eosio {

using eosio::asset;
using eosio::public_key;
using std::string;

void bos_oracle::regarbitrat( name account, public_key pubkey, uint8_t type, asset stake_amount, std::string public_info ) {
    require_auth( account );
    check( type == arbitrator_type::profession || type == arbitrator_type::amateur, "Arbitrator type can only be 1 or 2." );
    auto abr_table = arbitrators( get_self(), get_self().value );
    auto iter = abr_table.find( account.value );
    check( iter == abr_table.end(), "Arbitrator already registered" );
    transfer(account, arbitrat_account, stake_amount, "regarbitrat deposit.");

    abr_table.emplace( get_self(), [&]( auto& p ) {
        p.account = account;
        p.pubkey = pubkey;
        p.type = type;
        p.stake_amount = stake_amount;
        p.public_info = public_info;
    } );
}

void bos_oracle::complain( name applicant, uint64_t service_id, asset amount, std::string reason, uint8_t arbi_method ) {
    require_auth( applicant );
    check( arbi_method == arbi_method_type::crowd_arbitration || arbi_method_type::multiple_rounds, "`arbi_method` can only be 1 or 2." );

    data_services svctable(get_self(), get_self().value);
    auto svc_iter = svctable.find(service_id);
    check(svc_iter != svctable.end(), "service does not exist");
    check(svc_iter->status == service_status::service_in, "service status shoule be service_in");
    transfer(applicant, arbitrat_account, amount, "complain deposit.");

    auto complainant_tb = complainants( get_self(), get_self().value );
    auto complainant_by_svc = complainant_tb.template get_index<"svc"_n>();
    auto iter_compt = complainant_by_svc.find( service_id );
    auto is_sponsor = false;
    ///lisheng comment 空或申请结束两种情况又产生新的申诉
    if ( iter_compt == complainant_by_svc.end() ) {
        is_sponsor = true;
    } else {
        ///lisheng 正在仲裁中不接受对该服务  申诉
        check( iter_compt->status == complainant_status::wait_for_accept, "This complainant is not available." );
    }
    
    auto appeal_id = 0;
    complainant_tb.emplace( get_self(), [&]( auto& p ) {
        p.appeal_id = complainant_tb.available_primary_key();
        p.service_id = service_id;
        p.status = complainant_status::wait_for_accept;
        p.arbi_method = arbi_method;
        p.is_sponsor = is_sponsor;
        p.applicant = applicant;
        p.appeal_time = time_point_sec(now());
        p.reason = reason;
        appeal_id = p.appeal_id;
    } );

    // add_freeze
    const uint64_t duration = eosio::days(1);// microsecond unit
    add_delay(service_id, applicant, time_point_sec(now()), duration, amount);

    // Arbitration case application
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbicaseapp_tb_by_svc = arbicaseapp_tb.template get_index<"svc"_n>();
    auto arbicaseapp_iter = arbicaseapp_tb_by_svc.find( service_id );
    auto arbi_id = arbicaseapp_tb.available_primary_key();
    ///lisheng 不为空 插入
    if (arbicaseapp_iter == arbicaseapp_tb_by_svc.end() || 
        (arbicaseapp_iter != arbicaseapp_tb_by_svc.end() && arbicaseapp_iter->arbi_step == arbi_step_type::arbi_started)) {
        arbicaseapp_tb.emplace( get_self(), [&]( auto& p ) {
            p.arbitration_id = arbi_id;
            p.appeal_id = appeal_id;
            p.service_id = service_id;
            p.evidence_info = reason;
            p.arbi_step = arbi_step_type::arbi_init;
            p.required_arbitrator = 5;
            p.deadline = time_point_sec(now() + 3600);
            p.add_applicant(applicant);
        } );
    } else {
        auto arbi_iter = arbicaseapp_tb.find(arbicaseapp_iter->arbitration_id);
        check(arbi_iter != arbicaseapp_tb.end(), "Can not find such arbitration.")
        arbicaseapp_tb.modify(arbi_iter, get_self(), [&]( auto& p ) {
            p.add_applicant(applicant);
        }
    }

    // Data provider
    auto svcprovider_tb = data_service_provisions( get_self(), get_self().value );
    auto svcprovider_tb_by_svc = svcprovider_tb.template get_index<"bysvcid"_n>();
    auto svcprovider_iter = svcprovider_tb_by_svc.find( service_id );
    check(svcprovider_iter != svcprovider_tb_by_svc.end(), "Such service has no providers.");

    // Service data providers
    bool hasProvider = false;
    for(auto iter = svcprovider_tb.begin(); iter != svcprovider_tb.end(); iter++)
    {
        if(!svcprovider_iter->stop_service) {
            hasProvider = true;
            auto notify_amount = eosio::asset(1, _bos_symbol);
            // Transfer to provider
            auto memo = "arbitration_id: " + std::to_string(arbi_id)
                + ", service_id: " + std::to_string(service_id) 
                + ", state_amount: " + amount.to_string();
            transfer(get_self(), svcprovider_iter->account, notify_amount, memo);
        }
    }
    check(hasProvider, "no provier");
    timeout_deferred(arbi_id, arbitration_timer_type::resp_appeal_timeout, eosio::hour(10));

    //  auto notify_amount = eosio::asset(1, _bos_symbol);
    //         auto memo = "arbitration_id: " + std::to_string(arbicaseapp_iter->arbitration_id)
    //             + ", service_id: " + std::to_string(arbicaseapp_iter->service_id)
    //             + ", arbitration finished.";

    //         auto arbitrator_tb = arbitrators( get_self(), get_self().value );
    //         for (auto arbi : arbicaseapp_iter->arbitrators) {
    //             transfer(get_self(), arbi, notify_amount, memo);
    //             auto arbitrator_iter = arbitrator_tb.find(arbi.value);
    //             ///lisheng check it ==end ?
    //             //
    //             transfer(get_self(), arbi, arbitrator_iter->stake_amount, memo);
    //         }

        ///lisheng  是大于吗？ 逻辑位置不对
        // Check the last aribiration process time.   
        // if (arbicaseapp_iter->last_process_update_time.sec_since_epoch() + arbi_process_time_limit < now()) {
            // Someone complains about this arbitration.
        //     random_chose_arbitrator(arbicaseapp_iter->arbitration_id, arbicaseapp_iter->service_id);
        // } else {
            // // Find last process
            // auto arbiprocess_tb = arbitration_processs(get_self(), get_self().value);
            // auto arbiprocess_iter = arbiprocess_tb.find(arbicaseapp_iter->last_process_id);

            // ///lisheng check arbiprocess_iter== end
            // arbicaseapp_tb_by_svc.modify(arbicaseapp_iter, get_self(), [&]( auto& p ) {
            //     p.arbi_step = arbi_step_type::arbi_end;
            //     p.final_result = arbiprocess_iter->arbitration_result;
            // } );
            // auto notify_amount = eosio::asset(1, _bos_symbol);
            // auto memo = "arbitration_id: " + std::to_string(arbicaseapp_iter->arbitration_id)
            //     + ", service_id: " + std::to_string(arbicaseapp_iter->service_id)
            //     + ", arbitration finished.";

            // auto arbitrator_tb = arbitrators( get_self(), get_self().value );
            // for (auto arbi : arbicaseapp_iter->arbitrators) {
            //     transfer(get_self(), arbi, notify_amount, memo);
            //     auto arbitrator_iter = arbitrator_tb.find(arbi.value);
            //     ///lisheng check it ==end ?
            //     //
            //     ///transfer(get_self(), arbi, arbitrator_iter->stake_amount, memo);
            // }

            // for (auto applicant : arbicaseapp_iter->applicants) {
            //     transfer(get_self(), applicant, notify_amount, memo);
            // }

            ///lisheng 放到仲裁结束 更新正确率
            // Calculate arbitration correction.
            // update_arbitration_correcction(arbi_id);
        }
    }
}

void bos_oracle::uploadeviden( name applicant, uint64_t process_id, std::string evidence ) {
    require_auth( applicant );
    auto arbiprocess_tb = arbitration_processs( get_self(), get_self().value );
    auto arbipro_iter = arbiprocess_tb.find( process_id );
    check( arbipro_iter != arbiprocess_tb.end(), "Can not find such process.");

    arbiprocess_tb.modify( arbipro_iter, get_self(), [&]( auto& p ) {
        p.evidence_info = evidence;
    } );
}

void bos_oracle::uploadresult( name arbitrator, uint64_t arbitration_id, uint64_t result, uint64_t process_id ) {
    require_auth( arbitrator );
    check(result == 0 || result == 1, "`result` can only be 0 or 1.");

    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbi_iter = arbicaseapp_tb.find( arbitration_id );
    check(arbi_iter != arbicaseapp_tb.end(), "Can not find such arbitration case application.");
    check(arbi_iter->deadline >= time_point_sec(now()), "update result deadline.");

    auto arbiprocess_tb = arbitration_processs( get_self(), get_self().value );
    auto arbipro_iter = arbiprocess_tb.find( process_id );
    check( arbipro_iter != arbiprocess_tb.end(), "Can not find such process.");
    arbiprocess_tb.modify( arbipro_iter, get_self(), [&]( auto& p ) {
        p.add_result(result);
    } );

    // Calculate results
    if (arbipro_iter->result_size() >= arbi_iter->required_arbitrator / 2) {
       uint128_t deferred_id = make_deferred_id(arbitration_id, arbitration_timer_type::upload_result_timeout);
       cancel_deferred(deferred_id);
       handle_upload_result(arbitrator, arbitration_id, process_id);
    }
}

void bos_oracle::handle_upload_result(name arbitrator, uint64_t arbitration_id, uint64_t process_id)
{
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbi_iter = arbicaseapp_tb.find( arbitration_id );
    check(arbi_iter != arbicaseapp_tb.end(), "Can not find such arbitration case application.");

    auto arbiprocess_tb = arbitration_processs( get_self(), get_self().value );
    auto arbipro_iter = arbiprocess_tb.find( process_id );
    check( arbipro_iter != arbiprocess_tb.end(), "Can not find such process.");

    uint64_t arbi_result = 0;
    if (arbipro_iter->total_result() >= arbi_iter->required_arbitrator / 2) {
        arbi_result = 1;
    } else {
        arbi_result = 0;
    }
    arbiprocess_tb.modify( arbipro_iter, get_self(), [&]( auto& p ) {
        p.arbitration_result = arbi_result;
    } );

    // Add result to arbitration_results
    add_arbitration_result(arbitrator, arbitration_id, arbi_result, arbipro_iter->process_id);  
    timeout_deferred(arbitration_id, arbitration_timer_type::appeal_timeout, eosio::hour(10));
}

void bos_oracle::resparbitrat( name arbitrator, asset amount, uint64_t arbitration_id ) {
    require_auth( arbitrator );
    transfer(arbitrator, arbitrat_account, amount, "resparbitrat deposit.");

    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbi_iter = arbicaseapp_tb.find( arbitration_id );
    check(arbi_iter != arbicaseapp_tb.end(), "Can not find such arbitration.");
    arbicaseapp_tb.modify( arbi_iter, get_self(), [&]( auto& p ) {
        p.arbi_step = arbi_step_type::arbi_responded;
        p.add_arbitrator(arbitrator);
    } );

    // Check arbitrator number requirements.
    if (arbi_iter->arbitrators.size() >= arbi_iter->required_arbitrator) {
        arbicaseapp_tb.modify( arbi_iter, get_self(), [&]( auto& p ) {
            p.arbi_step = arbi_step_type::arbi_started;///还设置超时时间  上传结果 超时时间
        } );

        timeout_deferred(arbitration_id, arbitration_timer_type::upload_result_timeout, eosio::hour(10));
    } else {
        auto chose_arbi = arbi_iter->required_arbitrator - arbi_iter->arbitrators.size();
        random_chose_arbitrator(arbitration_id, arbi_iter->service_id, chose_arbi);
    }
}

void bos_oracle::reappeal(name applicant, uint64_t arbitration_id, uint64_t service_id, uint64_t result, uint64_t process_id, bool provider, asset amount)
{
  require_auth( applicant );
  
  data_services svctable(get_self(), get_self().value);
  auto svc_iter = svctable.find(service_id);
  check(svc_iter != svctable.end(), "service does not exist");
  check(svc_iter->status == service_status::service_in, "service status shoule be service_in");
  transfer(applicant, arbitrat_account, amount, "complain deposit.");
  if(provider) {
    // TODO
    // find compaint process_id
    // notify
  } else {
    // TODO
    // commplain(,bool isreappeal);
  }
  timeout_deferred(arbitration_id, arbitration_timer_type::resp_appeal_timeout, eosio::hour(10));
}

void rerespcase(name account.bool provider)
{
respcase()
}

void bos_oracle::respcase( name provider, uint64_t arbitration_id, uint64_t result, uint64_t process_id ,bool provider) {
    require_auth( provider );
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbi_iter = arbicaseapp_tb.find( arbitration_id );
    check(arbi_iter != arbicaseapp_tb.end(), "Can not find such arbitration case application.");
    check(status!= arbitrating,"")

    arbicaseapp_tb.modify( arbi_iter, get_self(), [&]( auto& p ) {
        p.arbi_step = arbi_iter->arbi_step + 1;
    } );

    auto complainant_tb = complainants( get_self(), get_self().value );
    auto complainant_iter = complainant_tb.find( arbi_iter->appeal_id );

    auto arbiprocess_tb = arbitration_processs( get_self(), get_self().value );
    auto arbipro_iter = arbiprocess_tb.find( process_id );
    if ( arbipro_iter == arbiprocess_tb.end() ) {
        arbiprocess_tb.emplace( get_self(), [&]( auto& p ) {
            p.process_id = arbiprocess_tb.available_primary_key();
            p.arbitration_id = arbitration_id;
            p.add_responder(arbitrator);
            p.num_id = 1; ///lisheng  不是在这加
        } );
        // ///放到 应诉方法中
    // start_arbitration(arbitrator_type::profession, arbi_id, service_id);
        start_arbitration();
    } else {
        arbiprocess_tb.modify( arbipro_iter, get_self(), [&]( auto& p ) {
            p.arbitration_id = arbitration_id;
            p.add_responder(arbitrator);
            // p.num_id += 1;///lisheng 不是在这加
        } );
    }
}

void bos_oracle::random_chose_arbitrator(uint64_t arbitration_id, uint64_t service_id) const {
    vector arbitrator = random_arbitrator(arbitration_id,chose_arbits);
    auto notify_amount = eosio::asset(1, _bos_symbol);
    // Transfer to arbitrator
    auto memo = "arbitration_id: " + std::to_string(arbitration_id)
        + ", service_id: " + std::to_string(service_id);
    transfer(get_self(), arbitrator, notify_amount, memo);
    arbicaseapp_tb.modify( arbi_iter, get_self(), [&]( auto& p ) {
        p.arbi_step = arbi_step_type::arbi_responded;
        p.add_arbitrator(arbitrator);
    } );

    timeout_deferred(arbitration_id, arbitration_timer_type::resp_arbitrate_timeout, eosio::hour(10));
}

void bos_oracle::start_arbitration(arbitrator_type arbitype, uint64_t arbitration_id, uint64_t service_id) {


    // for(requiared arbitrators)
    {
        status = choosing_arbitrator;
    random_chose_arbitrator(arbitration_id, service_id,n);
    }
}

vector bos_oracle::random_arbitrator(uint64_t arbitration_id,n) const {
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto iter_arbicaseapp = arbicaseapp_tb.find( arbitration_id );
    ///lisheng check iter == end
    auto chosen_arbitrators = iter_arbicaseapp->arbitrators;
    std::vector<name> chosen_from_arbitrators;

    //取掉选过的，  set
    auto arb_table = arbitrators( get_self(), get_self().value );
    for (auto iter = arb_table.begin(); iter != arb_table.end(); iter++)
    {
        
        auto chosen = std::find(chosen_arbitrators.begin(), chosen_arbitrators.end(), iter->account);
        if (chosen == chosen_arbitrators.end() && !iter->is_malicious) {
            chosen_from_arbitrators.push_back(iter->account);
        }
    }

for(set.size <n ){}

    auto total_arbi = chosen_from_arbitrators.size();
    auto tmp = tapos_block_prefix();
    auto arbi_id = random((void*)&tmp, sizeof(tmp));
    arbi_id %= total_arbi;
    .find(vector arbi_id 
    if(!=set.end)
{
    contiune;
}
    set.insert(

    )

    setidtonamevector

    return

    return chosen_from_arbitrators.at(set arbi_id);
}

void bos_oracle::add_arbitration_result(name arbitrator, uint64_t arbitration_id, uint64_t result, uint64_t process_id) {
    auto arbi_result_tb = arbitration_results( get_self(), get_self().value);
    arbi_result_tb.emplace( get_self(), [&]( auto& p ) {
        p.result_id = arbi_result_tb.available_primary_key();
        p.arbitration_id = arbitration_id;
        p.result = result;
        p.process_id = process_id;
        p.arbitrator = arbitrator;
    } );
}

void bos_oracle::update_arbitration_correcction(uint64_t arbitration_id) {
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbicaseapp_iter = arbicaseapp_tb.find(arbitration_id);
    check(arbicaseapp_iter != arbicaseapp_tb.end(), "Can not find such arbitration.");
    auto arbiresults_tb = arbitration_results( get_self(), get_self().value );
    auto arbitrator_tb = arbitrators( get_self(), get_self().value );

    auto arbitrators = arbicaseapp_iter->arbitrators;
    for (auto arbitrator : arbitrators) {
        uint64_t correct = 0;
        uint64_t total = 0;
        for (auto iter = arbiresults_tb.begin(); iter != arbiresults_tb.end(); iter++) {
            if (iter->arbitrator == arbitrator && iter->arbitration_id == arbitration_id) {
                total += 1;
                if (iter->result == arbicaseapp_iter->final_result) {
                    correct += 1;
                }
            }
        }

        double rate = correct > 0 && total > 0 ? 1.0 * correct / total : 0.0f;
        auto arbitrator_iter = arbitrator_tb.find(arbitrator.value);
        bool malicious = rate < bos_oracle::default_arbitration_correct_rate;

        arbitrator_tb.modify(arbitrator_iter, get_self(), [&]( auto& p ) {
            p.correct_rate = rate;
            p.is_malicious = malicious;
        } );
    }
}

uint128_t bos_oracle::make_deferred_id(uint64_t arbitration_id, arbitration_timer_type timer_type)
{
    return (uint128_t(arbitration_id) << 64) | timer_type;
}

void bos_oracle::timeout_deferred(uint64_t arbitration_id, arbitration_timer_type timer_type, uint64_t time_length)
{
    transaction t;
    t.actions.emplace_back(permission_level{_self, active_permission}, _self,
                           "timertimeout"_n,
                           std::make_tuple(arbitration_id, timer_type));
    t.delay_sec = time_length;
    uint128_t deferred_id = make_deferred_id(arbitration_id, timer_type);
    cancel_deferred(deferred_id);
    t.send(deferred_id, get_self());
}

void bos_oracle::timertimeout(uint64_t arbitration_id, arbitration_timer_type timer_type)
{
    auto arbicaseapp_tb = arbicaseapps( get_self(), get_self().value );
    auto arbicaseapp_iter = arbicaseapp_tb.find(arbitration_id);
    check(arbicaseapp_iter != arbicaseapp_tb.end(), "Can not find such arbitration.");

    switch(timer_type)
    {
        case arbitration_timer_type::appeal_timeout: {
            if(status ==wait for appeal against)
            {
                    handle_arbitration();
                    handle_arbitration_result();
            }
            break;
        }
        case arbitration_timer_type::resp_appeal_timeout: {
            //find if arbiration'status is 'wait for response'  set it's staus to timeout
            //appeal over  give  data prvoider fine amount from his stake amount, give  complaints bonus,
            if(arbicaseapp_iter->arbi_step == arbi_step_type::arbi_init) {
                handle_arbitration_result();
            }
            break;
        }

        case arbitration_timer_type::resp_arbitrate_timeout: {
            random chose  arbitrators 
            break;
        }

        case arbitration_timer_type::upload_result_timeout: {
            handle_upload_result();
            break;
        }
    }
}

void handle_arbitration()
{
    // Find last process
            auto arbiprocess_tb = arbitration_processs(get_self(), get_self().value);
            auto arbiprocess_iter = arbiprocess_tb.find(arbicaseapp_iter->last_process_id);

            ///lisheng check arbiprocess_iter== end
            arbicaseapp_tb_by_svc.modify(arbicaseapp_iter, get_self(), [&]( auto& p ) {
                p.arbi_step = arbi_step_type::arbi_end;
                p.final_result = arbiprocess_iter->arbitration_result;
            } );

 arbicaseapp_tb.modify( arbi_iter, get_self(), [&]( auto& p ) {
            p.last_process_update_time = time_point_sec(now()); 
            p.last_process_id = arbipro_iter->process_id;
        } );

            // auto notify_amount = eosio::asset(1, _bos_symbol);
            // auto memo = "arbitration_id: " + std::to_string(arbicaseapp_iter->arbitration_id)
            //     + ", service_id: " + std::to_string(arbicaseapp_iter->service_id)
            //     + ", arbitration finished.";

            // auto arbitrator_tb = arbitrators( get_self(), get_self().value );
            // for (auto arbi : arbicaseapp_iter->arbitrators) {
            //     transfer(get_self(), arbi, notify_amount, memo);
            //     auto arbitrator_iter = arbitrator_tb.find(arbi.value);
            //     ///lisheng check it ==end ?
            //     //
            //     ///transfer(get_self(), arbi, arbitrator_iter->stake_amount, memo);
            // }

            // for (auto applicant : arbicaseapp_iter->applicants) {
            //     transfer(get_self(), applicant, notify_amount, memo);
            // }



             // Calculate arbitration correction.
            update_arbitration_correcction(arbi_id);

}

void handle_arbitration_result
{
//appeal over  give  data prvoider fine amount from his stake amount, give  complaints bonus,
       
     // all  data provider stake amount minus  

}




