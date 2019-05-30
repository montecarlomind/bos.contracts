#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include "eosio.system_tester.hpp"


#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class bos_oracle_tester : public tester {
public:

   bos_oracle_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(oracle.bos),N(oraclize.bos),N(provider.bos),N(consumer.bos),N(riskctrl.bos)} );
      produce_blocks( 2 );

      set_code( N(oracle.bos), contracts::oracle_wasm() );
      set_abi( N(oracle.bos), contracts::oracle_abi().data() );
      set_code( N(dappuser.bos), contracts::dappuser_wasm() );
      set_abi( N(dappuser.bos), contracts::dappuser_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(oracle.bos) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer_max_time);
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(oracle.bos);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data,abi_serializer_max_time );

      return base_tester::push_action( std::move(act), uint64_t(signer));
   }
   
//provider
   fc::variant get_data_service( const uint64_t& service_id )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), N(oracle.bos), N(dataservices), service_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_fee( const uint64_t& service_id , const uint8_t& fee_type)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(servicefees), fee_type );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_fees", data, abi_serializer_max_time );
   }

   fc::variant get_provider( const name& account )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), N(oracle.bos), N(data_provider), account );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "providers", data, abi_serializer_max_time );
   }

   fc::variant get_provider_service( const name& account,const uint64_t& create_time_sec )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), account, N(provservices), create_time_sec );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "provider_service", data, abi_serializer_max_time );
   }

   uint64_t get_provider_service_id( const name& account,const uint64_t& create_time_sec )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), account, N(provservices), create_time_sec );
      return data.empty() ? 0 : abi_ser.binary_to_variant( "provider_service", data, abi_serializer_max_time )["service_id"].as<uint64_t>();
   }

   fc::variant get_data_service_provision( const uint64_t& service_id,const name& account)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(svcprovision), account );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_provision", data, abi_serializer_max_time );
   }

   fc::variant get_svc_provision_cancel_apply( const uint64_t& service_id , const name& provider)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(cancelapplys), provider );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "svc_provision_cancel_apply", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_provision_log( const uint64_t& service_id , const uint64_t& log_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(provisionlog), log_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_provision_log", data, abi_serializer_max_time );
   }

    fc::variant get_push_record( const uint64_t& service_id )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), N(oracle.bos), N(pushrecords), service_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "push_record", data, abi_serializer_max_time );
   }

   fc::variant get_provider_push_record( const uint64_t& service_id , const name& account)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(ppushrecords), account );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "provider_push_record", data, abi_serializer_max_time );
   }

   fc::variant get_action_push_record( const uint64_t& service_id, uint64_t key)
   {
      // uint64_t key = get_hash_key(get_nn_hash( contract_account, action_name);
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(apushrecords), key );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "action_push_record", data, abi_serializer_max_time );
   }

   fc::variant get_provider_action_push_record( const uint64_t& service_id , uint64_t key )
   {
      //   uint64_t key = get_hash_key(get_nnn_hash(account, contract_account, action_name);
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(papushrecord), key );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "provider_action_push_record", data, abi_serializer_max_time );
   }
//consumer
   fc::variant get_data_consumer(  const name& account)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), N(oracle.bos), N(dataconsumer), account );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_consumer", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_subscription( const uint64_t& service_id, uint64_t key )
   {
      // uint64_t key = get_hash_key(get_nn_hash( contract_account, action_name);
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(subscription), key );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_subscription", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_request( const uint64_t& service_id , const uint64_t& request_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(request), request_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_request", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_usage_record( const uint64_t& service_id , const uint64_t& usage_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(usagerecords), usage_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_usage_record", data, abi_serializer_max_time );
   }


   fc::variant get_service_consumptions( const uint64_t& service_id )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(consumptions), service_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "service_consumptions", data, abi_serializer_max_time );
   }
//risk control
   fc::variant get_riskcontrol_account( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(oracle.bos), acc, N(accounts), symbol_code );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "riskcontrol_account", data, abi_serializer_max_time );
   }

   fc::variant get_data_service_stake( const uint64_t& service_id )
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(servicestake), service_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "data_service_stake", data, abi_serializer_max_time );
   }

   fc::variant get_transfer_freeze_delay( const uint64_t& service_id , const uint64_t& transfer_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(usagerecords), transfer_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "transfer_freeze_delay", data, abi_serializer_max_time );
   }

   fc::variant get_risk_guarantee( const uint64_t& service_id , const uint64_t& risk_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(usagerecords), risk_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "risk_guarantee", data, abi_serializer_max_time );
   }

   fc::variant get_account_freeze_log( const uint64_t& service_id , const uint64_t& log_id)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(freezelog), log_id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account_freeze_log", data, abi_serializer_max_time );
   }

  fc::variant get_account_freeze_stat( const uint64_t& service_id , const name& account)
   {
      vector<char> data = get_row_by_account( N(oracle.bos), service_id, N(freezestats), account );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account_freeze_stat", data, abi_serializer_max_time );
   }

   fc::variant get_service_freeze_stat(const uint64_t &service_id) {
     vector<char> data = get_row_by_account(N(oracle.bos), service_id,
                                            N(svcfrozestat), service_id);
     return data.empty()
                ? fc::variant()
                : abi_ser.binary_to_variant("service_freeze_stat", data,
                                            abi_serializer_max_time);
   }

//provider
   action_result regservice(
      uint64_t service_id, name account, asset stake_amount,
      asset service_price, uint64_t fee_type, std::string data_format,
      uint64_t data_type, std::string criteria, uint64_t acceptance,
      std::string declaration, uint64_t injection_method,
      uint64_t duration, uint64_t provider_limit, uint64_t update_cycle,
      time_point_sec update_start_time) {

      return push_action( N(oracle.bos), N(regservice), mvo()
           ( "service_id", service_id)
           ( "account", account)
           ( "stake_amount", stake_amount)
           ( "service_price", service_price)
           ( "fee_type", fee_type)
           ( "data_format", data_format)
           ( "data_type", data_type)
           ( "criteria", criteria)
           ( "acceptance", acceptance)
           ( "declaration", declaration)
           ( "injection_method", injection_method)
           ( "duration", duration)
           ( "provider_limit", provider_limit)
           ( "update_cycle", update_cycle)
           ( "update_start_time", update_start_time)
      );
   }

   action_result unregservice(uint64_t service_id,
                                      name account,
                                      uint64_t is_suspense) {
      return push_action(  N(oracle.bos), N(unregservice), mvo()
           ( "service_id", service_id)
           ( "account", account)
           ( "is_suspense", is_suspense)
      );
   }

   action_result execaction(uint64_t service_id, uint64_t action_type) {
      return push_action( N(oracle.bos), N(execaction), mvo()
           ( "service_id", service_id)
           ( "action_type", action_type)
      );

   }

   action_result stakeamount( uint64_t service_id, 
                                     name account, 
                                     asset stake_amount){
      return push_action( N(oracle.bos), N(stakeamount), mvo()
           ( "service_id", service_id)
           ( "account", account)
           ( "stake_amount", stake_amount)
      );
   }

   action_result pushdata(uint64_t service_id, name provider,
                                  name contract_account, name action_name,
                                   uint64_t request_id,const string& data_json){
      return push_action( N(oracle.bos), N(pushdata), mvo()
           ( "service_id", service_id )
           ( "provider", provider )
           ( "contract_account", contract_account )
           ( "action_name", action_name )
           ( "request_id", request_id)
           ( "data_json", data_json )
      );
   }

   action_result multipush(uint64_t service_id, name provider,
                                  const string& data_json, bool is_request){
      return push_action( N(oracle.bos), N(multipush), mvo()
           ( "service_id", service_id )
           ( "provider", provider )
           ( "data_json", data_json )
           ( "is_request", is_request )
      );
   }

   action_result addfeetype(uint64_t service_id,
                                    std::vector<uint8_t> fee_types,
                                    std::vector<asset> service_prices) {
      return push_action( N(oracle.bos), N(addfeetype), mvo()
           ( "service_id", service_id )
           ( "fee_types", fee_types )
           ( "service_prices", service_prices )
      );
   }

  action_result claim(name account, name receive_account) {
      return push_action( N(oracle.bos), N(claim), mvo()
           ( "account", account )
           ( "receive_account", receive_account )
      );
   }
//consumer
   action_result subscribe(uint64_t service_id, name contract_account,
                           name action_name, std::string publickey,
                           name account, asset amount, std::string memo) {
     return push_action(
         N(oracle.bos), N(subscribe),mvo()
         ("service_id", service_id)
         ("contract_account", contract_account)
         ("action_name", action_name)
         ("publickey", publickey)
         ("account", account)
         ("amount", amount)
         ("memo", memo)
         );
   }

   action_result requestdata(uint64_t service_id, name contract_account,
                                     name action_name, name requester,
                                     std::string request_content) {
     return push_action(
         N(oracle.bos), N(requestdata),mvo()
         ("service_id", service_id)
         ("contract_account", contract_account)
         ("action_name", action_name)
         ("requester", requester)
         ("request_content", request_content)
         );
   }

   action_result payservice(uint64_t service_id, name contract_account,
                                    name action_name, name account,
                                    asset amount, std::string memo) {
     return push_action(
         N(oracle.bos), N(payservice),mvo()
         ("service_id", service_id)
         ("contract_account", contract_account)
         ("action_name", action_name)
         ("account", account)
         ("amount", amount)
         ("memo", memo)
         );
   }


   action_result confirmpay(uint64_t service_id, name contract_account,
                                    name action_name, asset amount) {
     return push_action(
         N(oracle.bos), N(confirmpay),mvo()
         ("service_id", service_id)
         ("contract_account", contract_account)
         ("action_name", action_name)
         ("amount", amount)
         );
   }

//riskcontrol
   action_result deposit(name from, name to, asset quantity,
                                 string memo, bool is_notify) {
     return push_action(
         N(oracle.bos), N(deposit),mvo()
         ("from", from)
         ("to", to)
         ("quantity", quantity)      
         ("memo", memo)
         ("is_notify", is_notify)
         );
   }

  action_result withdraw(name from, name to, asset quantity,
                                 string memo) {
     return push_action(
         N(oracle.bos), N(withdraw),mvo()
         ("from", from)
         ("to", to)
         ("quantity", quantity)      
         ("memo", memo)
         );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(bos_oracle_tests)

BOOST_FIXTURE_TEST_CASE( reg_test, bos_oracle_tester ) try {

 name account = N(alice);
 uint64_t service_id =0;
  uint8_t fee_type = 1;
  uint8_t data_type = 1;
  uint8_t status = 0;
  uint8_t injection_method = 0;
  uint64_t acceptance = 0;
  uint64_t duration = 1;
  uint64_t provider_limit = 3;
  uint64_t update_cycle = 1;
  uint64_t appeal_freeze_period = 1;
  uint64_t exceeded_risk_control_freeze_period = 1;
  uint64_t guarantee_id = 0;
  asset service_price = asset::from_string("10 EOS");
  asset stake_amount = asset::from_string("1000 EOS");
  asset risk_control_amount = asset::from_string("0 EOS");
  asset pause_service_stake_amount = asset::from_string("0 EOS");
  std::string data_format = "";
  std::string criteria = "";
  std::string declaration = "";
  bool freeze_flag = false;
  bool emergency_flag = false;
  time_point_sec update_start_time = time_point_sec( control->head_block_time() );



   auto token = regservice(service_id,  account,
                             stake_amount,  service_price,
                             fee_type, data_format,
                             data_type,  criteria,
                             acceptance,  declaration,
                             injection_method,  duration,
                             provider_limit,  update_cycle,
                             update_start_time);
   
   uint64_t new_service_id = get_provider_service_id(account,static_cast<uint64_t>(update_start_time.sec_since_epoch()));
   auto services = get_data_service(new_service_id);
   REQUIRE_MATCHING_OBJECT( services, mvo()
   ( "service_id", service_id)
           ( "account", account)
           ( "stake_amount", stake_amount)
           ( "service_price", service_price)
           ( "fee_type", fee_type)
           ( "data_format", data_format)
           ( "data_type", data_type)
           ( "criteria", criteria)
           ( "acceptance", acceptance)
           ( "declaration", declaration)
           ( "injection_method", injection_method)
           ( "duration", duration)
           ( "provider_limit", provider_limit)
           ( "update_cycle", update_cycle)
           ( "update_start_time", update_start_time)
           ( "appeal_freeze_period", appeal_freeze_period)
           ( "exceeded_risk_control_freeze_period", exceeded_risk_control_freeze_period)
           ( "guarantee_id", guarantee_id)
           ( "risk_control_amount", risk_control_amount)
           ( "pause_service_stake_amount", pause_service_stake_amount)
           ( "freeze_flag", freeze_flag)
           ( "emergency_flag", emergency_flag)
           ( "status", status)
   );
   produce_blocks(1);

  status = 2;
   
  auto unregedservice = unregservice(service_id, account, status);
   auto unregedservices = get_data_service(new_service_id);
     REQUIRE_MATCHING_OBJECT( services, mvo()
   ( "service_id", service_id)
           ( "account", account)
           ( "stake_amount", stake_amount)
           ( "service_price", service_price)
           ( "fee_type", fee_type)
           ( "data_format", data_format)
           ( "data_type", data_type)
           ( "criteria", criteria)
           ( "acceptance", acceptance)
           ( "declaration", declaration)
           ( "injection_method", injection_method)
           ( "duration", duration)
           ( "provider_limit", provider_limit)
           ( "update_cycle", update_cycle)
           ( "update_start_time", update_start_time)
           ( "appeal_freeze_period", appeal_freeze_period)
           ( "exceeded_risk_control_freeze_period", exceeded_risk_control_freeze_period)
           ( "guarantee_id", guarantee_id)
           ( "risk_control_amount", risk_control_amount)
           ( "pause_service_stake_amount", pause_service_stake_amount)
           ( "freeze_flag", freeze_flag)
           ( "emergency_flag", emergency_flag)
           ( "status", status)
 );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( unreg_freeze_test, bos_oracle_tester ) try {

  uint64_t service_id = 1;
  name account = N(alice);
  uint8_t status = 2;
 
   // auto services = get_data_service(service_id);
//   auto token = unregservice(service_id, account, status);
    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "provider does not subscribe service" ),
                        unregservice(service_id, account, status)
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( execaction_test, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  uint8_t action_type = 2;
 
  auto token = execaction(service_id,  action_type);

   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( stakeamount_test, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  name account = N(alice);
  asset stake_amount = asset::from_string("0 EOS");
  
  auto token = stakeamount(service_id, account, stake_amount);


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_test, bos_oracle_tester ) try {
name account = N(alice);
name receive_account = N(alice);
auto token = claim(account,receive_account);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( pushdata_test, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  name provider = N(alice);
  name contract_account = N(alice);
  name action_name = N(alice);
  const string data_json = "";
  uint64_t request_id = 0;

  auto token = pushdata(service_id,  provider,
                           contract_account,  action_name,  request_id,
                           data_json);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( multipush_tests, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  name provider = N(alice);
  const string data_json = "";
  uint64_t request_id = 0;

  auto token = multipush(service_id,  provider,
                     data_json,  request_id);
   produce_blocks(1);

  

   

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( addfeetype_tests, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  std::vector<uint8_t> fee_types = {};
  std::vector<asset> service_prices = {};
  auto token = addfeetype(service_id, fee_types, service_prices);
  produce_blocks(1);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( subscribe_tests, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  name contract_account = N(alice);
  name action_name = N(alice);
  std::string publickey = "";
  name account = N(alice);
  asset amount = asset::from_string("1000 EOS");
  std::string memo = "";
  auto token = subscribe(service_id,  contract_account,
                            action_name,  publickey,
                            account,  amount,  memo);


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( confirmpay_tests, bos_oracle_tester ) try {
 uint64_t service_id = 0;
  name contract_account = N(alice);
  name action_name = N(alice);
  name account = N(alice);
  asset amount = asset::from_string("1000 EOS");
  std::string memo = "";
  auto token = confirmpay(service_id,  contract_account,
                            action_name, amount);



} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( requestdata_tests, bos_oracle_tester ) try {

  uint64_t service_id = 0;
  name contract_account = N(alice);
  name action_name = N(alice);
  name account = N(alice);
  std::string request_content = "";
  auto token = requestdata(service_id,  contract_account,
                            action_name,  
                            account,    request_content);


} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( deposit_tests, bos_oracle_tester ) try {

  name from = N(alice);
  name to = N(alice);
  asset quantity = asset::from_string("1000 EOS");
  std::string memo = "";
bool is_notify = false;
  auto token = deposit(from,  to,
                             quantity,  memo,is_notify);

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( withdraw_tests, bos_oracle_tester ) try {
 uint64_t service_id = 0;
  name from = N(alice);
  name to = N(alice);
  asset quantity = asset::from_string("1000 EOS");
  std::string memo = "";
  auto token = withdraw(from,  to,
                             quantity,  memo);

} FC_LOG_AND_RETHROW()



BOOST_AUTO_TEST_SUITE_END()