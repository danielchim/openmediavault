<?php
/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @copyright Copyright (c) 2009-2016 Volker Theile
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */
namespace OMV\Rpc\Proxy;

require_once("openmediavault/util.inc");
require_once("openmediavault/functions.inc");
require_once("openmediavault/rpc.inc");

/**
 * The generic RPC proxy implementation that handles JSON RPC requests.
 * A RPC request must contain the fields:
 *   \em service The name of the service.
 *   \em method The name of the method.
 *   \em params The method parameters as JSON string.
 *   \em options An optional array of RPC options with the fields:
 *     \em updatelastaccess Set to FALSE to do not update the time on which
 *     the last access took place.
 * @ingroup api
 */
class Json extends \OMV\BaseObject {
	protected $params = NULL;

	/**
	 * Handle the RPC request.
	 * @return The RPC response as JSON encoded data.
	 */
	public function handle() {
		// Get the RPC params.
		if (FALSE === $this->getParams())
			throw new Exception("Failed to get RPC parameters");

		// Validate RPC parameters.
		$this->validate();

		// Process optional RPC options.
		$updateLastAccess = TRUE;
		if (array_key_exists("options", $this->params) && !is_null(
		  $this->params['options']) && array_key_exists("updatelastaccess",
		  $this->params['options'])) {
			$updateLastAccess = boolvalEx(
			  $this->params['options']['updatelastaccess']);
		}

		// Check and update the last accessed time only if the
		// session is authenticated. Every additional check will be
		// done later if required. This is because there are several
		// RPCs that must be executed without an authenticated
		// session, e.g. the 'Login' RPC.
		$session = &\OMV\Session::getInstance();
		if ($session->isAuthenticated()) {
			$session->validateTimeout();
			if (TRUE === $updateLastAccess)
				$session->updateLastAccess();
		}

		// Check if it is a local or remote RPC. If it is a RPC that is
		// redirected from the WebGUI backend to the omv-engined daemon,
		// then we can commit the session (these RPCs are executed in
		// another process thus they can not access this session).
		$rpcServiceMngr = &\OMV\Rpc\ServiceManager::getInstance();
		if (FALSE === ($rpcService = $rpcServiceMngr->getService(
		  $this->params['service']))) {
			// Session MUST be authenticated at this point.
			$session->validateAuthentication();
			// If service is not available locally, then we can commit
			// the session due the fact that the RPC is redirected to
			// the omv-engined daemon which does not have access to this
			// session.
			$session->commit();
		}

		// Execute the RPC.
		$this->preExecute();
		$context = \OMV\Rpc\Rpc::createContext($session->getUsername(),
		  $session->getRole());
		$response = \OMV\Rpc\Rpc::call($this->params['service'],
		  $this->params['method'], $this->params['params'], $context,
		  OMV_RPC_MODE_LOCAL | OMV_RPC_MODE_REMOTE);
		$this->postExecute($response);

		// Print response.
		$this->handleResponse($response);
	}

	/**
	 * Get the RPC parameters.
	 * @return TRUE if successful, otherwise FALSE.
	 */
	protected function getParams() {
		// See http://in.php.net/wrappers.php for more details
		$this->params = json_decode(file_get_contents("php://input"), TRUE);
		return !empty($this->params);
	}

	/**
	 * Validate the RPC parameters. An exception is thrown when
	 * the validation fails.
	 * @return None.
	 */
	protected function validate() {
		$schema = new \OMV\Json\Schema('{
			"type":"object",
			"properties":{
				"service":{"type":"string","required":true},
				"method":{"type":"string","required":true},
				"params":{"type":"any","required":true},
				"options":{"type":"any","required":false}
			}
		}');
		$schema->validate(json_encode_safe($this->params));
	}

	/**
	 * Do additional stuff before RPC is executed.
	 * @return None.
	 */
	protected function preExecute() {
		// Nothing to do here.
	}

	/**
	 * Do additional stuff after RPC has been executed.
	 * @param response The RPC response
	 * @return None.
	 */
	protected function postExecute(&$response) {
		// Nothing to do here.
	}

	/**
	 * Print the RPC responce.
	 * @param response The RPC response
	 * @return None.
	 */
	protected function handleResponse($response) {
		header("Content-Type: application/json");
		print json_encode_safe(array(
			"response" => $response,
			"error" => null
		));
	}

	/**
	 * The function that is called after the RPC has been successfully
	 * executed or it has been failed.
	 */
	public function cleanup() {
		// Nothing to do here.
	}
}