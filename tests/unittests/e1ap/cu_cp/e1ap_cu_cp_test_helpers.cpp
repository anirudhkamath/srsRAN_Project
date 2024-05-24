/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "e1ap_cu_cp_test_helpers.h"
#include "srsran/support/async/async_test_utils.h"
#include "srsran/support/test_utils.h"

using namespace srsran;
using namespace srs_cu_cp;

e1ap_cu_cp_test::e1ap_cu_cp_test()
{
  test_logger.set_level(srslog::basic_levels::debug);
  e1ap_logger.set_level(srslog::basic_levels::debug);
  srslog::init();

  e1ap = create_e1ap(
      e1ap_pdu_notifier, cu_up_processor_notifier, cu_cp_notifier, ue_mng, timers, ctrl_worker, max_nof_supported_ues);
}

e1ap_cu_cp_test::~e1ap_cu_cp_test()
{
  // flush logger after each test
  srslog::flush();
}

void e1ap_cu_cp_test::run_bearer_context_setup(ue_index_t ue_index, gnb_cu_up_ue_e1ap_id_t cu_up_ue_e1ap_id)
{
  e1ap_bearer_context_setup_request req = generate_bearer_context_setup_request(ue_index);

  // Start procedure in CU-CP.
  async_task<e1ap_bearer_context_setup_response>         t = e1ap->handle_bearer_context_setup_request(req);
  lazy_task_launcher<e1ap_bearer_context_setup_response> t_launcher(t);

  ASSERT_FALSE(t.ready());

  test_ues.emplace(ue_index, test_ue{ue_index});
  test_ue& u         = test_ues[ue_index];
  u.ue_index         = ue_index;
  u.cu_cp_ue_e1ap_id = int_to_gnb_cu_cp_ue_e1ap_id(
      this->e1ap_pdu_notifier.last_e1ap_msg.pdu.init_msg().value.bearer_context_setup_request()->gnb_cu_cp_ue_e1ap_id);
  u.cu_up_ue_e1ap_id = cu_up_ue_e1ap_id;

  // Handle response from CU-UP.
  e1ap_message bearer_context_setup_response =
      generate_bearer_context_setup_response(u.cu_cp_ue_e1ap_id.value(), cu_up_ue_e1ap_id);
  test_logger.info("Injecting BearerContextSetupResponse");
  e1ap->handle_message(bearer_context_setup_response);

  srsran_assert(t.ready(), "The procedure should have completed by now");
}

e1ap_cu_cp_test::test_ue& e1ap_cu_cp_test::create_ue()
{
  ue_index_t ue_index = ue_mng.add_ue(du_index_t::min);
  auto       request  = generate_bearer_context_setup_request(ue_index);

  run_bearer_context_setup(request.ue_index,
                           int_to_gnb_cu_up_ue_e1ap_id(test_rgen::uniform_int<uint64_t>(
                               gnb_cu_up_ue_e1ap_id_to_uint(gnb_cu_up_ue_e1ap_id_t::min),
                               gnb_cu_up_ue_e1ap_id_to_uint(gnb_cu_up_ue_e1ap_id_t::max) - 1)));

  return test_ues[request.ue_index];
}

void e1ap_cu_cp_test::tick()
{
  timers.tick();
  ctrl_worker.run_pending_tasks();
}
